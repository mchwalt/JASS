#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <map>
#include "Parameters.h"
#include "PresetIO.h"   // setPresetLoading / applySeqLatchRoot / seqLatchRoot hooks

// STEP SEQ ⇄ Standard MIDI File (story 15.8). Import turns a .mid transcription (Los Niños,
// Der Mussolini, basic-pitch output) into the running figure; export writes the figure back
// out so it travels to a DAW. Both live behind the existing LOAD/SAVE dialogs — the file
// extension decides, no new rack UI.
//
// The SMF contract (15.6 market analysis, confirmed by this project's measurements):
//   · Position and velocity are ground truth. The grid is 1/16 (PPQ/4 ticks per step),
//     anchored on the FIRST note-on — transcriptions rarely start at tick 0.
//   · Velocity ⇒ accent by CLUSTERING, never read continuously: a spread ≥ 8 splits at the
//     midpoint and the upper class becomes the accents (Los Niños: v98 against v80/86).
//   · Duration ⇒ gate/TIE/SLIDE: within its step ⇒ percent of the step; holding through
//     following EMPTY steps ⇒ those steps are synthesized (same pitch) and chained with TIE;
//     overlapping the NEXT note-on ⇒ SLIDE — the 303 convention. Ending exactly ON the next
//     onset is a plain 100 % step (legato retrigger — DAF), NOT a tie.
//   · Cycle detection: a transcription loops its figure many times. The smallest period p
//     (2..32 steps, at least two full cycles seen) is the figure; velocities and durations
//     are FOLDED across the cycles (median), which also averages out transcription noise.
//   · Root = the figure's most frequent note (the pedal). It lands in the LATCH, so an
//     imported figure starts playing exactly like a loaded sequencer preset.
namespace SeqMidiIO
{
    using APVTS = juce::AudioProcessorValueTreeState;

    enum class Error { none, unreadable, smpteTime, noNotes, notQuantized };

    struct ImportResult
    {
        Error  error   = Error::none;
        int    steps   = 0;      // figure length (LEN after import)
        int    root    = -1;     // latched root note
        double tempo   = 0.0;    // BPM from the file's first tempo event, 0 = none carried
    };

    namespace detail
    {
        struct Ev { double step; int note; int vel; double lenSteps; };

        inline void setRaw (APVTS& a, const juce::String& id, float raw)
        {
            if (auto* p = a.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (raw));
        }

        // One figure position after folding: what gets written into the step row.
        struct Fold { int note = -1; juce::Array<int> vels; juce::Array<double> lens; };

        inline double median (juce::Array<double>& v)
        {
            v.sort();
            return v[v.size() / 2];
        }
    }

    // ── Import ───────────────────────────────────────────────────────────────────────────
    inline ImportResult importFigure (APVTS& a, const juce::File& file)
    {
        using namespace detail;
        ImportResult r;

        juce::FileInputStream in (file);
        juce::MidiFile midi;
        if (! in.openedOk() || ! midi.readFrom (in))
        {
            r.error = Error::unreadable;
            return r;
        }
        const short fmt = midi.getTimeFormat();
        if (fmt <= 0)   // SMPTE frames — none of our sources write it; the grid math assumes PPQ
        {
            r.error = Error::smpteTime;
            return r;
        }
        const double stepTicks = fmt / 4.0;   // 1/16 grid

        // Merge every track: note pairs + the first tempo event. basic-pitch writes one track,
        // hand transcriptions sometimes split hands — the figure is monophonic either way.
        juce::Array<Ev> evs;
        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*midi.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto& m = seq.getEventPointer (i)->message;
                if (m.isTempoMetaEvent() && r.tempo == 0.0)
                    r.tempo = 60.0 / m.getTempoSecondsPerQuarterNote();
                if (! m.isNoteOn())
                    continue;
                const double off = seq.getTimeOfMatchingKeyUp (i);
                const double len = off > m.getTimeStamp() ? (off - m.getTimeStamp()) / stepTicks
                                                          : 0.5;   // unmatched note-on: half a step
                evs.add ({ m.getTimeStamp() / stepTicks, m.getNoteNumber(),
                           (int) m.getVelocity(), len });
            }
        }
        if (evs.isEmpty())
        {
            r.error = Error::noNotes;
            return r;
        }

        std::sort (evs.begin(), evs.end(), [] (const Ev& x, const Ev& y) { return x.step < y.step; });
        const double anchor = evs.getReference (0).step;

        // Free-time material is rejected LOUDLY, not mangled: a raw whole-song transcription
        // (basic-pitch at its default 120 BPM) has onsets nowhere near any 1/16 grid, and
        // quantizing those is inventing a figure the file does not contain. Uniformly random
        // offsets average 0.25 steps from the grid; a quantized loop sits at ~0. (Found with
        // exactly such a file, 2026-08-30 — it imported as doubled-note hash before this.)
        {
            double dev = 0.0;
            for (auto& e : evs)
            {
                const double p = e.step - anchor;
                dev += std::abs (p - (double) std::llround (p));
            }
            if (dev / evs.size() > 0.2)
            {
                r.error = Error::notQuantized;
                return r;
            }
        }

        // Quantize onto the 1/16 grid, anchored on the first onset. One note per step: the
        // louder one wins (a transcription chord is an artefact; the sequencer is monophonic).
        std::map<int, Ev> grid;
        for (auto& e : evs)
        {
            const int s = (int) std::llround (e.step - anchor);
            if (s < 0) continue;
            auto it = grid.find (s);
            if (it == grid.end() || e.vel > it->second.vel)
                grid[s] = e;
        }
        const int span = grid.rbegin()->first + 1;

        // Cycle detection on positions + notes only (velocity and duration wobble per pass in a
        // real transcription — they are folded below instead of compared here).
        int period = juce::jmin (span, 32);
        bool looped = false;
        for (int p = 2; p <= 32 && p < span; ++p)
        {
            if (span < 2 * p) break;   // need at least two full cycles to trust a period
            bool ok = true;
            for (int s = 0; s + p < span && ok; ++s)
            {
                const auto x = grid.find (s), y = grid.find (s + p);
                const bool hx = x != grid.end(), hy = y != grid.end();
                ok = (hx == hy) && (! hx || x->second.note == y->second.note);
            }
            if (ok) { period = p; looped = true; break; }
        }

        // Fold every cycle onto the figure: median duration, all velocities kept for clustering.
        // ONLY when a real period was found — folding a non-looping file mod 32 would pile the
        // whole piece onto 32 steps. Without a cycle the figure is the first 32 sixteenths,
        // exactly the fallback the contract names.
        std::map<int, Fold> fig;
        for (auto& [s, e] : grid)
        {
            if (! looped && s >= period) break;   // grid is an ordered map
            auto& f = fig[s % period];
            f.note = e.note;
            f.vels.add (e.vel);
            f.lens.add (e.lenSteps);
        }

        // Velocity ⇒ accent by clustering across the whole figure.
        int vmin = 127, vmax = 1;
        for (auto& [q, f] : fig)
            for (int v : f.vels) { vmin = juce::jmin (vmin, v); vmax = juce::jmax (vmax, v); }
        const bool twoClasses = (vmax - vmin) >= 8;
        const double vSplit = (vmax + vmin) / 2.0;

        // Root = most frequent note.
        std::map<int, int> hist;
        for (auto& [q, f] : fig) hist[f.note] += f.vels.size();
        int root = fig.begin()->second.note, best = 0;
        for (auto& [n, c] : hist) if (c > best) { best = c; root = n; }

        // Resolve durations into the per-step gate row. gate[q]: 5..100 = percent, 101 = TIE,
        // 102 = SLIDE. Synthesized held steps extend `on`/`pitch` beyond the played onsets.
        std::map<int, int> on, pitch, acc, gate;
        for (auto& [q, f] : fig)
        {
            on[q]    = 1;
            pitch[q] = juce::jlimit (-24, 24, f.note - root);
            int vmed; { juce::Array<double> tmp; for (int v : f.vels) tmp.add (v); vmed = (int) median (tmp); }
            acc[q]   = (twoClasses && vmed > vSplit) ? 1 : 0;
            juce::Array<double> tmp (f.lens); const double len = median (tmp);

            // Next onset within the cycle (wrapping): the reference for legato/overlap. A
            // single-note figure finds itself one period later, which is exactly right.
            int nq = 0;
            for (int d = 1; d <= period; ++d)
                if (fig.count ((q + d) % period)) { nq = d; break; }
            const double eps = 0.1;   // a tenth of a step: transcription jitter, not intent

            // How many steps the note occupies, and what the LAST of them does at its end.
            int covered, lastGate;
            if (len > nq + eps)                     { covered = nq; lastGate = 102; }   // SLIDE into the next onset
            else if (std::abs (len - nq) <= eps)    { covered = nq; lastGate = 100; }   // legato retrigger (DAF), NOT a tie
            else
            {
                covered  = juce::jlimit (1, nq, (int) std::ceil (len - eps));
                lastGate = juce::jlimit (5, 100, (int) std::llround ((len - (covered - 1)) * 100.0));
            }
            covered = juce::jmin (covered, period);   // never wrap past one full cycle
            for (int i = 0; i < covered; ++i)
            {
                const int st = (q + i) % period;
                if (i > 0)   // a synthesized held step: same pitch, plain, only there to be TIEd into
                {
                    on[st] = 1;
                    if (! pitch.count (st)) pitch[st] = pitch[q];
                    if (! acc.count (st))   acc[st] = 0;
                }
                gate[st] = (i < covered - 1) ? 101 : lastGate;   // 101 = TIE chains the holds
            }
        }

        // Write the figure — inside the preset-loading bracket (PR #60): couplings silent,
        // voices killed on both edges, so half-applied steps never sound.
        if (PresetIO::setPresetLoading) PresetIO::setPresetLoading (true);
        for (int s = 1; s <= 32; ++s)   // full reset first: import replaces the WHOLE figure
        {
            const int q = s - 1;
            setRaw (a, Parameters::ID::seqStep  (s), on.count (q)    ? 1.0f : 0.0f);
            setRaw (a, Parameters::ID::seqPitch (s), on.count (q)    ? (float) pitch[q] : 0.0f);
            setRaw (a, Parameters::ID::seqAcc   (s), acc.count (q) && acc[q] ? 1.0f : 0.0f);
            setRaw (a, Parameters::ID::seqSGate (s), gate.count (q)  ? (float) gate[q] : 100.0f);
        }
        setRaw (a, Parameters::ID::seqLength, (float) period);
        setRaw (a, Parameters::ID::seqSync, 5.0f);    // "1/16" — the contract's grid
        setRaw (a, Parameters::ID::seqGate, 1.0f);    // measured percents assume an unscaled gate
        setRaw (a, Parameters::ID::seqOn, 1.0f);      // the ARP exclusion coupling handles the rest
        if (r.tempo > 0.0)
            setRaw (a, Parameters::ID::syncTempo, (float) juce::jlimit (40.0, 250.0, r.tempo));
        if (PresetIO::setPresetLoading) PresetIO::setPresetLoading (false);
        if (PresetIO::applySeqLatchRoot) PresetIO::applySeqLatchRoot (root);   // …and it plays

        r.steps = period;
        r.root  = root;
        return r;
    }

    // ── Export ───────────────────────────────────────────────────────────────────────────
    // One cycle, 480 PPQ (120 ticks per 1/16 step). Velocities are exactly what the engine
    // emits (127 accented / 100 plain). TIE chains merge into one note; a pitch takeover (TIE
    // or SLIDE with a new pitch) exports as the 303's OVERLAP — documented asymmetry: a
    // pitch-changing TIE reimports as SLIDE, because MIDI cannot carry the difference.
    inline bool exportFigure (APVTS& a, const juce::File& file)
    {
        constexpr int ppq = 480, stepTicks = ppq / 4, overlap = 30;
        auto raw = [&a] (const juce::String& id) { return a.getRawParameterValue (id)->load(); };

        const int len  = juce::jlimit (1, 32, (int) raw (Parameters::ID::seqLength));
        const int root = PresetIO::seqLatchRoot && PresetIO::seqLatchRoot() >= 0
                             ? PresetIO::seqLatchRoot() : 48;   // C3 — the same fallback the preset writer uses

        juce::MidiMessageSequence seq;
        {
            const double bpm = raw (Parameters::ID::syncTempo);
            seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60.0e6 / bpm)), 0.0);
        }

        auto stepOn    = [&] (int s) { return raw (Parameters::ID::seqStep  (s + 1)) > 0.5f; };
        auto stepNote  = [&] (int s) { return juce::jlimit (0, 127, root + (int) raw (Parameters::ID::seqPitch (s + 1))); };
        auto stepAcc   = [&] (int s) { return raw (Parameters::ID::seqAcc   (s + 1)) > 0.5f; };
        auto stepGate  = [&] (int s) { return (int) raw (Parameters::ID::seqSGate (s + 1)); };

        bool wroteNote = false;
        int s = 0;
        while (s < len)
        {
            if (! stepOn (s)) { ++s; continue; }
            // A chain: this step plus every following step it TIEs/SLIDEs into. A pitch change
            // inside the chain closes the running note with an overlap and opens the next one.
            int noteStart = s * stepTicks;
            int note = stepNote (s);
            int vel  = stepAcc (s) ? 127 : 100;
            while (true)
            {
                const int g = stepGate (s);
                const bool chains = (g >= 101) && (s + 1 < len) && stepOn (s + 1);
                if (! chains)
                {
                    const int pct = (g >= 101) ? 100 : juce::jlimit (5, 100, g);   // chain end at LEN: full step
                    const int end = s * stepTicks + stepTicks * pct / 100;
                    seq.addEvent (juce::MidiMessage::noteOn  (1, note, (juce::uint8) vel), noteStart);
                    seq.addEvent (juce::MidiMessage::noteOff (1, note), end);
                    wroteNote = true;
                    ++s;
                    break;
                }
                if (stepNote (s + 1) != note)
                {
                    // Takeover: close with the 303 overlap, the new pitch starts ON the boundary.
                    const int boundary = (s + 1) * stepTicks;
                    seq.addEvent (juce::MidiMessage::noteOn  (1, note, (juce::uint8) vel), noteStart);
                    seq.addEvent (juce::MidiMessage::noteOff (1, note), boundary + overlap);
                    wroteNote = true;
                    noteStart = boundary;
                    note = stepNote (s + 1);
                    vel  = stepAcc (s + 1) ? 127 : 100;
                }
                ++s;   // same pitch: the TIE simply extends the running note
            }
        }
        if (! wroteNote)
            return false;   // an all-rest figure exports nothing a reader could use

        seq.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (ppq);
        midi.addTrack (seq);
        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && midi.writeTo (out);
    }
}
