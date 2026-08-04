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
//      hikey / pitch_keycenter / lovel / hivel (velocity layers, 12.5) / offset / ampeg_release /
//      amp_veltrack / volume / tune. EVERY other opcode is ignored, silently.
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

    // One imported mapping entry, before the audio is loaded. Since 12.5 velocity is a real
    // ZONE DIMENSION (loVel/hiVel select the layer at note-on) — the old "loudest layer wins"
    // dedupe is gone.
    struct Entry
    {
        juce::File file;
        int rootKey = 60;
        int loKey   = 0;
        int hiKey   = 127;
        int hiVel   = 127;
        int offsetFrames = 0;   // sfz offset= — skip this many frames at the file start
        float releaseSeconds = -1.0f;   // sfz ampeg_release= — note-off fade (12.4); <0 ⇒ unset
        int   loVel = 0;                // sfz lovel= — velocity layer lower bound (12.5)
        float veltrack = 0.0f;          // sfz amp_veltrack= 0..100 — velocity→gain amount (12.5);
                                        //   entriesFromSfz defaults it to 100 (spec), folders to 0
        float volumeDb  = 0.0f;         // sfz volume= — per-zone gain in dB (12.5)
        int   tuneCents = 0;            // sfz tune= — per-zone pitch offset in cents (12.5)
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
    // numbers), lovel= / hivel= (velocity LAYERS — real zone dimension since 12.5), offset=,
    // ampeg_release= (12.4), amp_veltrack= / volume= / tune= (12.5), default_path=. Everything
    // else is ignored. Missing pitch_keycenter ⇒ 60, missing amp_veltrack ⇒ 100 (both the SFZ
    // defaults — D1 in story 12.5: imported .sfz sets respond to touch). "//" starts a comment.
    // Robustness rules:
    //   · headers glued to their first opcode ("<region>sample=x", legal SFZ) are split;
    //   · a region with an UNPARSABLE key/lokey/hikey/pitch_keycenter value is DROPPED — the
    //     old "treat as unset" fallback turned a typo into a full-keyboard region that shadowed
    //     every other zone (zoneFor is first-match);
    //   · regions covered by an earlier region in KEY *and* VELOCITY are dropped (first-match
    //     zoneFor could never reach them; they'd only charge the caps).
    inline constexpr float kUnsetF = -1.0e9f;      // "opcode absent" sentinels (0 is a valid value)
    inline constexpr int   kUnsetI = -100000;

    inline std::vector<Entry> entriesFromSfz (const juce::File& sfzFile)
    {
        struct Scope
        {
            juce::String sample;
            int lo = -1, hi = -1, root = -1, hiVel = -1, offset = -1;
            float rel = -1.0f;
            int   loVel = -1;             // 12.5 velocity layer bounds
            float vt    = -1.0f;          // 12.5 amp_veltrack (percent); <0 ⇒ unset
            float vol   = kUnsetF;        // 12.5 volume (dB)
            int   tune  = kUnsetI;        // 12.5 tune (cents)
            bool bad = false;
        };
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
            int loVel = region.loVel >= 0 ? region.loVel : group.loVel;
            int hiVel = region.hiVel >= 0 ? region.hiVel : group.hiVel;
            int offs  = region.offset >= 0 ? region.offset : group.offset;
            float rel = region.rel   >= 0.0f ? region.rel : group.rel;   // <0 stays "unset"
            float vt  = region.vt    >= 0.0f ? region.vt  : group.vt;
            float vol = region.vol  != kUnsetF ? region.vol  : group.vol;
            int  tune = region.tune != kUnsetI ? region.tune : group.tune;
            if (root  < 0) root  = 60;    // SFZ default: unchanged on middle C
            if (lo    < 0) lo    = 0;
            if (hi    < 0) hi    = 127;
            if (loVel < 0) loVel = 0;
            if (hiVel < 0) hiVel = 127;
            if (offs  < 0) offs  = 0;
            if (vt    < 0.0f) vt = 100.0f;   // D1 (12.5): .sfz sets track velocity per spec default
            if (vol  == kUnsetF) vol  = 0.0f;
            if (tune == kUnsetI) tune = 0;
            if (! region.bad && ! group.bad && sample.isNotEmpty() && lo <= hi && loVel <= hiVel)
            {
                Entry fresh;
                fresh.file    = baseDir.getChildFile ((defaultPath + sample).replaceCharacter ('\\', '/'));
                fresh.rootKey = juce::jlimit (0, 127, root);
                fresh.loKey   = lo;
                fresh.hiKey   = hi;
                fresh.loVel   = loVel;
                fresh.hiVel   = hiVel;
                fresh.offsetFrames   = offs;
                fresh.releaseSeconds = rel;
                fresh.veltrack  = vt;
                fresh.volumeDb  = vol;
                fresh.tuneCents = tune;
                // 12.5 dedupe: velocity is a zone dimension now. Drop a region only when an
                // earlier one covers it in KEY *and* VELOCITY (true duplicate/subset — zoneFor
                // is first-match, so it could never sound); distinct layers always survive.
                bool shadowed = false;
                for (const auto& e : entries)
                    if (e.loKey <= lo && e.hiKey >= hi && e.loVel <= loVel && e.hiVel >= hiVel)
                    { shadowed = true; break; }
                if (! shadowed)
                    entries.push_back (std::move (fresh));
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
                else if (opcode == "lovel")           // velocity layer bounds (12.5: real zones)
                    s.loVel = juce::jlimit (0, 127, value.getIntValue());
                else if (opcode == "hivel")
                    s.hiVel = juce::jlimit (0, 127, value.getIntValue());
                else if (opcode == "offset")          // skip pre-attack junk the library trims
                    s.offset = juce::jmax (0, value.getIntValue());
                else if (opcode == "ampeg_release")   // note-off fade in seconds (Story 12.4);
                {                                     // ≤0 / garbage parses to 0 ⇒ treated as unset
                    const float v = value.getFloatValue();
                    if (v > 0.0f) s.rel = juce::jmin (v, 30.0f);
                }
                else if (opcode == "amp_veltrack")    // velocity→gain amount (12.5); negative
                    s.vt = juce::jlimit (0.0f, 100.0f, value.getFloatValue());   // tracking: unsupported
                else if (opcode == "volume")          // per-zone gain in dB (12.5: layer balancing)
                    s.vol = juce::jlimit (-60.0f, 12.0f, value.getFloatValue());
                else if (opcode == "tune")            // per-zone cents (12.5: stretch tuning)
                    s.tune = juce::jlimit (-1200, 1200, value.getIntValue());
                // every other opcode: ignored by design (minimal subset)
            }
        }
        flushRegion();
        return entries;
    }
}
