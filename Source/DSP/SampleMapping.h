#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>

// ── SAMPLER key-mapping import (Story 12.2) ──────────────────────────────────────────────────
// JASS derives or imports multisample mappings, it never authors them (the 12.1 analysis: only
// AUTHORING needs an editor). Two sources, both reduced to the same result — a list of
// (file, rootKey, loKey, hiKey):
//   1. a folder of files named  <anything>_<note>.wav  (note = "C3"/"A#4" or a MIDI number),
//      ranges split halfway between neighbouring roots;
//   2. a minimal .sfz subset — <group>/<region> headers and the opcodes sample / key / lokey /
//      hikey / pitch_keycenter. EVERY other opcode is ignored, silently.
// Note names resolve with C4 = MIDI 60 — the pitch-model convention used everywhere in JASS
// (keyboard labelling, "FREQ knobs define the sound AT C4"). MESSAGE THREAD only.
namespace SampleMapping
{
    // "C3" / "A#4" / "Db2" / "60" → MIDI note, or -1 if unparsable. C4 = 60.
    inline int parseNoteToken (const juce::String& raw)
    {
        auto t = raw.trim();
        if (t.isEmpty())
            return -1;
        if (t.containsOnly ("0123456789"))
        {
            const int v = t.getIntValue();
            return (v >= 0 && v <= 127) ? v : -1;
        }
        static constexpr int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A..G
        const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);
        if (letter < 'A' || letter > 'G')
            return -1;
        int semi = base[letter - 'A'];
        int i = 1;
        if (i < t.length() && t[i] == '#')      { ++semi; ++i; }
        else if (i < t.length() && t[i] == 'b') { --semi; ++i; }   // lowercase only ('B' is a note)
        // Octave must be a well-FORMED integer (optional leading '-', then digits only) — a
        // set-membership check let "C1-2" slip through as C1 (review 2026-08-04).
        auto oct = t.substring (i);
        const bool neg = oct.startsWithChar ('-');
        const auto digits = neg ? oct.substring (1) : oct;
        if (digits.isEmpty() || ! digits.containsOnly ("0123456789"))
            return -1;
        const int midi = (oct.getIntValue() + 1) * 12 + semi;      // C4 = (4+1)*12 = 60
        return (midi >= 0 && midi <= 127) ? midi : -1;
    }

    // Root key from a filename following the convention "<anything>_<note>.<ext>". -1 if the
    // name has no '_' or the suffix is not a note.
    inline int parseRootFromFilename (const juce::String& fileNameNoExt)
    {
        if (! fileNameNoExt.containsChar ('_'))
            return -1;
        return parseNoteToken (fileNameNoExt.fromLastOccurrenceOf ("_", false, false));
    }

    // One imported mapping entry, before the audio is loaded. hiVel ranks velocity layers of
    // the same key range (the LOUDEST layer wins the dedupe — see entriesFromSfz).
    struct Entry
    {
        juce::File file;
        int rootKey = 60;
        int loKey   = 0;
        int hiKey   = 127;
        int hiVel   = 127;
        int offsetFrames = 0;   // sfz offset= — skip this many frames at the file start
    };

    // Audio extensions the sampler accepts everywhere (LOAD dialog, folder scan, preload).
    // FLAC decodes natively via juce::AudioFormatManager::registerBasicFormats — added so the
    // big free .sfz libraries (Salamander, Splendid Grand, ...) load without conversion.
    inline constexpr const char* kAudioWildcard = "*.wav;*.aif;*.aiff;*.flac";

    // Derive lo/hi for entries that only carry a root (folder convention): sort by root, split
    // halfway between neighbours, outermost zones extend to 0/127. Duplicate roots: first wins.
    inline void deriveRanges (std::vector<Entry>& entries)
    {
        // stable_sort: keeps the alphabetical pre-sort for equal roots, so "duplicate roots:
        // first (alphabetical) wins" is actually guaranteed, not STL-implementation luck.
        std::stable_sort (entries.begin(), entries.end(),
                          [] (const Entry& a, const Entry& b) { return a.rootKey < b.rootKey; });
        entries.erase (std::unique (entries.begin(), entries.end(),
                                    [] (const Entry& a, const Entry& b) { return a.rootKey == b.rootKey; }),
                       entries.end());
        for (size_t i = 0; i < entries.size(); ++i)
        {
            entries[i].loKey = (i == 0) ? 0
                                        : (entries[i - 1].rootKey + entries[i].rootKey + 1) / 2;
            entries[i].hiKey = (i + 1 == entries.size())
                                        ? 127
                                        : (entries[i].rootKey + entries[i + 1].rootKey + 1) / 2 - 1;
        }
    }

    // Scan a folder for "<anything>_<note>" audio files → entries with derived ranges. Files
    // without a parsable note suffix are skipped (a mixed folder still yields its mapped part).
    inline std::vector<Entry> entriesFromFolder (const juce::File& dir)
    {
        std::vector<Entry> entries;
        auto files = dir.findChildFiles (juce::File::findFiles, false, kAudioWildcard);
        files.sort();
        for (const auto& f : files)
            if (const int root = parseRootFromFilename (f.getFileNameWithoutExtension()); root >= 0)
                entries.push_back ({ f, root, 0, 127 });
        deriveRanges (entries);
        return entries;
    }

    // ── Minimal .sfz parser (Story 12.2 AC3, hardened by review 2026-08-04) ──────────────────
    // Recognised: <group>/<global>/<master> (defaults that inherit into regions), <region>, and
    // <control> (ONLY for default_path= — very common in real files); any other header suspends
    // parsing until the next known one. Opcodes: sample= (value may contain spaces — runs until
    // the next token containing '='), key= / lokey= / hikey= / pitch_keycenter= (note names or
    // numbers), default_path=. Everything else is ignored. Missing pitch_keycenter ⇒ 60 (the
    // SFZ default). "//" starts a comment. Robustness rules:
    //   · headers glued to their first opcode ("<region>sample=x", legal SFZ) are split;
    //   · a region with an UNPARSABLE key/lokey/hikey/pitch_keycenter value is DROPPED — the
    //     old "treat as unset" fallback turned a typo into a full-keyboard region that shadowed
    //     every other zone (zoneFor is first-match);
    //   · regions whose key range is CONTAINED in an earlier region's range are dropped —
    //     velocity layers (lovel/hivel, unsupported) would otherwise load N copies that charge
    //     the caps but can never be reached.
    inline std::vector<Entry> entriesFromSfz (const juce::File& sfzFile)
    {
        struct Scope { juce::String sample; int lo = -1, hi = -1, root = -1, hiVel = -1, offset = -1; bool bad = false; };
        Scope group, region;
        bool inRegion = false, ignoring = false, inControl = false;
        juce::String defaultPath;
        std::vector<Entry> entries;
        const auto baseDir = sfzFile.getParentDirectory();

        auto flushRegion = [&]
        {
            if (! inRegion)
                return;
            const juce::String sample = region.sample.isNotEmpty() ? region.sample : group.sample;
            int lo    = region.lo    >= 0 ? region.lo    : group.lo;
            int hi    = region.hi    >= 0 ? region.hi    : group.hi;
            int root  = region.root  >= 0 ? region.root  : group.root;
            int hiVel = region.hiVel >= 0 ? region.hiVel : group.hiVel;
            int offs  = region.offset >= 0 ? region.offset : group.offset;
            if (root  < 0) root  = 60;    // SFZ default: unchanged on middle C
            if (lo    < 0) lo    = 0;
            if (hi    < 0) hi    = 127;
            if (hiVel < 0) hiVel = 127;
            if (offs  < 0) offs  = 0;
            if (! region.bad && ! group.bad && sample.isNotEmpty() && lo <= hi)
            {
                Entry fresh { baseDir.getChildFile ((defaultPath + sample).replaceCharacter ('\\', '/')),
                              juce::jlimit (0, 127, root), lo, hi, hiVel, offs };
                bool handled = false;
                for (auto& e : entries)
                {
                    if (e.loKey == lo && e.hiKey == hi)
                    {   // velocity layer of the SAME zone: keep the loudest one (hivel ranks it)
                        if (fresh.hiVel > e.hiVel) e = fresh;
                        handled = true;
                        break;
                    }
                    if (e.loKey <= lo && e.hiKey >= hi) { handled = true; break; }   // shadowed
                }
                if (! handled)
                    entries.push_back (fresh);
            }
            inRegion = false;
            region = {};
        };

        juce::StringArray lines;
        lines.addLines (sfzFile.loadFileAsString());
        for (auto line : lines)
        {
            line = line.upToFirstOccurrenceOf ("//", false, false)
                       .replace (">", "> ");   // split headers glued to their first opcode
            juce::StringArray tokens;
            tokens.addTokens (line, " \t", "");
            tokens.removeEmptyStrings();
            for (int i = 0; i < tokens.size(); ++i)
            {
                const auto& tok = tokens.getReference (i);
                if (tok.startsWithChar ('<'))
                {
                    flushRegion();
                    inControl = false;
                    if (tok == "<region>")      { inRegion = true; ignoring = false; }
                    else if (tok == "<group>" || tok == "<global>" || tok == "<master>")
                                                { group = {}; ignoring = false; }
                    else if (tok == "<control>"){ ignoring = false; inControl = true; }
                    else                        { ignoring = true; }
                    continue;
                }
                if (ignoring || ! tok.contains ("="))
                    continue;
                const auto opcode = tok.upToFirstOccurrenceOf ("=", false, false).toLowerCase();
                auto value        = tok.fromFirstOccurrenceOf ("=", false, false);
                if (opcode == "sample" || opcode == "default_path")   // value may contain spaces
                    while (i + 1 < tokens.size() && ! tokens.getReference (i + 1).contains ("="))
                        value << " " << tokens.getReference (++i);

                if (inControl)
                {
                    if (opcode == "default_path")
                        defaultPath = value.endsWithChar ('/') || value.endsWithChar ('\\')
                                          || value.isEmpty() ? value : value + "/";
                    continue;   // no other <control> opcode is interpreted
                }

                Scope& s = inRegion ? region : group;
                auto note = [&s] (const juce::String& v)
                {
                    const int k = parseNoteToken (v);
                    if (k < 0) s.bad = true;   // typo ⇒ drop the region, never widen it
                    return k;
                };
                if      (opcode == "sample")          s.sample = value;
                else if (opcode == "lokey")           s.lo = note (value);
                else if (opcode == "hikey")           s.hi = note (value);
                else if (opcode == "pitch_keycenter") s.root = note (value);
                else if (opcode == "key")             s.lo = s.hi = s.root = note (value);
                else if (opcode == "hivel")           // ranks velocity layers only (see flush)
                    s.hiVel = juce::jlimit (0, 127, value.getIntValue());
                else if (opcode == "offset")          // skip pre-attack junk the library trims
                    s.offset = juce::jmax (0, value.getIntValue());
                // every other opcode: ignored by design (minimal subset)
            }
        }
        flushRegion();
        return entries;
    }
}
