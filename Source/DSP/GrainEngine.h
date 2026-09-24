#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "Hermite.h"
#include "ScaleMask.h"

// ─── GRAIN — granular cloud on the SAMPLER's material (Story 17.1) ──────────────────────────────
// One instance per voice. Plays a fixed pool of short Hann-windowed grains out of a sample zone
// (raw channel pointers at the file's own rate — the SampleBankStore never frees, so the pointers
// stay valid for the life of the app). Three dimensions per grain: WHERE (POS ± SPRAY), HOW LONG
// (SIZE), HOW FAR TRANSPOSED (per-grain pitch, drawn uniformly over scale *degrees* when QUANT is
// on). Density is grains per second.
//
// This is a texture generator, not a pitch-shifter — Story 12.3 measured a naive granular
// repitcher at negative SNR and chose signalsmith-stretch for that job; here the artefacts are the
// sound, and SPRAY breaks the periodic combing that made them ugly there.
//
// KEY mode (Story 17.2): the grain rate follows the played note's frequency instead of DENS, and
// the grain CONTENT is no longer transposed by the key — the pitch you hear is the repetition rate
// (FOF / VOSIM / pitch-synchronous granular), the sample's own formants stay where they are. SIZE
// is then the formant bandwidth, and is capped so the pool never drops a grain (a dropped grain in
// a periodic train halves the pitch). Found as the SPRAY-0 regime in the Task-1 measurement.
//
// RT contract: no allocation ever (fixed arrays), no locks, one cos-recurrence per grain instead of
// a cos() per sample. Pool full ⇒ the new grain is DROPPED, never stolen (a stolen grain is cut
// mid-window = a click per steal). JUCE-free: the scratch harness compiles this header alone.
class GrainEngine
{
public:
    static constexpr int    kMaxGrains     = 32;
    static constexpr double kKeyMaxPeriods = 2.0;   // KEY mode: grain length ≤ this many periods of the note

    struct Out { float l, r; };

    // ── configuration (prepareToPlay) ──────────────────────────────────────────────────────────
    void setSampleRate (double sr) noexcept
    {
        hostSampleRate = sr > 1000.0 ? sr : 44100.0;
        reset();
    }

    // ── per-block parameters (applyToVoice) ────────────────────────────────────────────────────
    void setEnabled (bool on) noexcept                { enabled = on; }
    void setPosition (double frac) noexcept           { position = std::clamp (frac, 0.0, 1.0); }
    void setSpray (double frac) noexcept              { spray = std::clamp (frac, 0.0, 1.0); }
    void setSizeMs (double ms) noexcept               { sizeMs = std::clamp (ms, 1.0, 2000.0); }
    // A DENS move up must not wait out the period already scheduled at the old rate (1 s at DENS 1,
    // review 2026-09-24): pull the pending onset in to the new period. Down needs nothing — the
    // longer period simply starts with the next spawn. KEY mode ignores DENS altogether.
    void setDensity (double grainsPerSecond) noexcept
    {
        density = std::clamp (grainsPerSecond, 0.1, 1000.0);
        if (! keyMode) nextOnset = std::min (nextOnset, hostSampleRate / density);
    }
    void setPitchSpread (double semis) noexcept       { pitchSpread = std::clamp (semis, 0.0, 48.0); }
    void setPitchCenter (double semis) noexcept       { pitchCenter = std::clamp (semis, -48.0, 48.0); }
    void setQuant (int q) noexcept                    { quant = std::clamp (q, 0, 4); }
    void setLevel (double lvl) noexcept               { level = std::clamp (lvl, 0.0, 2.0); }
    void setKeyMode (bool on) noexcept                { keyMode = on; }

    // Loudness normalisation against the expected overlap o = length · rate: gain = o^-exp.
    // 0.5 is the incoherent-sum law (power adds), 1.0 the coherent one (amplitude adds). Measured
    // 2026-09-21 (scratch harness, EPiano C4 mono + CH_01 stereo, DENS 5…200 × SIZE 5…300 ms ×
    // SPRAY 0/10/50 %): with o^-0.5 the dense regime (o ≥ 1) sits within a 2.7 dB span across the
    // whole grid; o^-0.75 drifts to −18 dB. The residual −4.3 dB is the Hann window's own power
    // (mean w² = 3/8), compensated by sqrt(8/3) so a dense cloud lands at the source's loudness for
    // the same LEVEL as the SAMPLER. The sparse regime (o < 1: clicks with silence between) stays
    // quieter by nature. SPRAY 0 at high overlap is the pitch-synchronous regime — see KEY mode.
    void setNormExponent (double e) noexcept          { normExp = std::clamp (e, 0.0, 1.0); }

    // Note transposition as a rate factor: f(note)/f(zone root) · tune — the same `pitchFactor`
    // SamplePlayer::computeRate builds, so PITCH 0 + QUANT Off is exactly the sampler's tape pitch.
    void setPitchFactor (double f) noexcept           { pitchFactor = std::clamp (f, 1.0 / 64.0, 64.0); }

    // The played note's absolute frequency (KEY mode: the grain rate). Set at note-on and followed
    // through GLIDE / SLIDE per sample. The cap covers MIDI 127 (12 544 Hz — an 8 kHz cap left the
    // top eight keys on one pitch, review 2026-09-24); the length floor of 8 samples holds below it.
    void setNoteHz (double hz) noexcept               { noteHz = std::clamp (hz, 8.0, 13000.0); }

    // ── material (note-on; pointers must outlive the grains — the store guarantees it) ─────────
    void setMaterial (const float* left, const float* right, int frames, double fileSampleRate) noexcept
    {
        matL = left;
        matR = (right != nullptr) ? right : left;
        matFrames = (left != nullptr) ? frames : 0;
        fileRate = fileSampleRate > 1000.0 ? fileSampleRate : 44100.0;
    }

    // Per-voice seed (once, at construction): every voice must draw its own SPRAY / pitch sequence,
    // or a chord is several sample-identical clouds summing coherently (+6 dB, no thickening).
    void seed (uint32_t s) noexcept                   { rng = (s == 0) ? 0x9E3779B9u : s; }

    // ── note lifecycle ─────────────────────────────────────────────────────────────────────────
    // Note-on: the first grain fires on the very next sample (attack on time). Grains still in
    // flight from the previous note on this voice finish naturally — cutting them would click.
    void trigger() noexcept                           { nextOnset = 0.0; scheduling = true; }

    // Note-off WITHOUT an envelope to shape the tail (ADSR module off): stop starting new grains,
    // let the sounding ones finish. With the ADSR on this is never called — scheduling runs through
    // the release like an OSC keeps oscillating, and the envelope shapes the cloud.
    void stopScheduling() noexcept                    { scheduling = false; }

    // Hard stop (voice reset): empties the pool.
    void reset() noexcept
    {
        for (auto& g : grains) g.active = false;
        nextOnset = 0.0;
        scheduling = true;
    }

    // ── render ─────────────────────────────────────────────────────────────────────────────────
    Out nextSample() noexcept
    {
        // Only the SCHEDULER is gated by enable / material / note-off: grains already in flight
        // carry their own pointers and finish their window regardless (review 2026-09-24 — an
        // early return here cut the whole cloud on one sample when GRAIN was switched off or a
        // zone-less note arrived, the very click trigger() promises not to make).
        if (scheduling && enabled && matFrames >= 8)
        {
            // Scheduler: fractional period accumulator, so 44100/30 = 1470.0 grains/s stays exact.
            // KEY mode: one grain per period of the played note — the rate IS the pitch.
            nextOnset -= 1.0;
            if (nextOnset <= 0.0)
            {
                spawn();
                nextOnset += hostSampleRate / grainRate();
                if (nextOnset < 1.0) nextOnset = 1.0;   // never more than one spawn per sample
            }
        }

        double outL = 0.0, outR = 0.0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const double w = g.gain * 0.5 * (1.0 - g.c0);  // Hann via cos recurrence × the grain's own norm
            const float sL = hermiteRead (g.l, g.n, g.pos);
            const float sR = (g.r == g.l) ? sL : hermiteRead (g.r, g.n, g.pos);
            outL += w * sL;
            outR += w * sR;
            const double cn = g.k * g.c1 - g.c0;            // cos((age+1)·step)
            g.c0 = g.c1; g.c1 = cn;
            g.pos += g.rate;
            if (++g.age >= g.length) g.active = false;
        }
        // LEVEL stays a live per-sample factor (AMP modulation must breathe); the overlap norm is
        // baked into each grain at spawn, so turning DENS or SIZE never steps the grains already
        // sounding — the maintainer heard exactly that zipper on the first play (2026-09-21).
        const float gain = (float) level;
        return { (float) outL * gain, (float) outR * gain };
    }

    // ── base values for the matrix (SynthVoice captures per block, modulates, restores) ───────
    double getPosition() const noexcept    { return position; }
    double getSizeMs() const noexcept      { return sizeMs; }
    double getLevel() const noexcept       { return level; }
    double getPitchCenter() const noexcept { return pitchCenter; }

    // ── introspection (harness / display) ──────────────────────────────────────────────────────
    int activeGrains() const noexcept
    {
        int c = 0;
        for (const auto& g : grains) c += g.active ? 1 : 0;
        return c;
    }
    uint64_t spawnedCount() const noexcept { return spawned; }
    uint64_t droppedCount() const noexcept { return dropped; }
    double   currentNorm() const noexcept  { return lastNorm; }
    double   grainRate() const noexcept    { return keyMode ? noteHz : density; }   // grains per second

    // The per-grain pitch draw, exposed so the harness can histogram it. QUANT off: continuous
    // spread around the centre. QUANT on: an integer scale step, uniform over the degrees within
    // ±spread of the (snapped) centre — up and down counted separately, the scale is not symmetric.
    static double drawOffsetSemis (uint32_t& rngState, double centerSemis, double spreadSemis, int q) noexcept
    {
        if (q <= 0)
            return centerSemis + spreadSemis * bipolar (rngState);
        const int cs   = ScaleMask::nearestStep (centerSemis, q);
        const int up   = ScaleMask::stepsUpWithin (cs, spreadSemis, q);
        const int down = ScaleMask::stepsDownWithin (cs, spreadSemis, q);
        const int span = up + down + 1;
        const int step = cs - down + (int) (next (rngState) % (uint32_t) span);
        return ScaleMask::stepToSemis (step, q);
    }

private:
    struct Grain
    {
        const float* l = nullptr;
        const float* r = nullptr;
        int    n = 0;
        double pos = 0.0, rate = 1.0;
        int    length = 0, age = 0;
        double c0 = 1.0, c1 = 1.0, k = 2.0;   // cos recurrence: c[age] = cos(age·2π/length)
        double gain = 1.0;                    // overlap norm at spawn time (see nextSample)
        bool   active = false;
    };

    static uint32_t next (uint32_t& s) noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    static double bipolar (uint32_t& s) noexcept   // uniform in [-1, 1)
    {
        return (double) (next (s) >> 8) * (2.0 / 16777216.0) - 1.0;
    }

    void spawn() noexcept
    {
        Grain* g = nullptr;
        for (auto& c : grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) { ++dropped; return; }   // pool full: drop, never steal

        const double rate_ = grainRate();
        int length = (int) std::lround (sizeMs * 0.001 * hostSampleRate);
        length = std::max (length, 8);
        if (keyMode)
        {
            // Two caps. (1) Overlap < kMaxGrains so the periodic train is never thinned — a dropped
            // grain in KEY mode would halve the pitch. (2) At most kKeyMaxPeriods periods of the
            // note: measured 2026-09-22 on the EPiano C4 sample, a grain longer than ~2 periods
            // resolves the SOURCE's own partials (window narrower than their spacing), and the
            // note's harmonics then fall between them — −15 dB at 440 Hz for 10 ms, −31 dB for
            // 20 ms, −40 dB at 880 Hz. At 2 periods the level stays within ±1.5 dB from 110 to
            // 880 Hz (constant-Q formant). SIZE therefore acts up to 2/f: 18 ms at 110 Hz, 4.5 ms
            // at 440 Hz. The help says so.
            const int capPool    = (int) ((double) (kMaxGrains - 1) * hostSampleRate / rate_);
            const int capPeriods = (int) (kKeyMaxPeriods * hostSampleRate / rate_);
            length = std::max (8, std::min ({ length, capPool, capPeriods }));
        }

        const double offsetSemis = drawOffsetSemis (rng, pitchCenter, pitchSpread, quant);
        // KEY mode reads the content at the file's own speed (formants stay put; the rate is the
        // pitch); cloud mode transposes it with the key like the sampler's tape path.
        const double contentFactor = keyMode ? 1.0 : pitchFactor;
        const double rate = contentFactor * std::exp2 (offsetSemis / 12.0) * (fileRate / hostSampleRate);

        // Keep the whole read span inside the material: hermiteRead clamps and cannot overrun, but
        // a grain hanging over the end would read the held edge sample — a Hann-shaped DC bump.
        const double maxSpan = (double) (matFrames - 1) - 4.0;
        double span = length * rate + 4.0;
        if (span > maxSpan)
        {
            length = std::max (8, (int) ((maxSpan - 4.0) / rate));
            span = length * rate + 4.0;
            if (span > maxSpan) { ++dropped; return; }   // material shorter than one grain
        }
        const double centre = position * (double) (matFrames - 1);
        const double start  = std::clamp (centre + spray * (double) (matFrames - 1) * bipolar (rng),
                                          0.0, maxSpan - span);

        // Overlap norm for THIS grain (see setNormExponent): sqrt(8/3) · o^-exp, o = length · rate.
        constexpr double kHannPowerComp = 1.6329931618554521;   // sqrt(8/3)
        const double overlap = std::max (1.0, (double) length / hostSampleRate * rate_);
        lastNorm = kHannPowerComp * std::pow (overlap, -normExp);

        g->l = matL; g->r = matR; g->n = matFrames;
        g->pos = start; g->rate = rate;
        g->length = length; g->age = 0;
        g->gain = lastNorm;   // frozen for this grain's life; DENS/SIZE moves only affect new grains
        const double step = 6.283185307179586 / (double) length;
        g->c0 = 1.0; g->c1 = std::cos (step); g->k = 2.0 * g->c1;
        g->active = true;
        ++spawned;
    }

    std::array<Grain, kMaxGrains> grains {};
    const float* matL = nullptr;
    const float* matR = nullptr;
    int    matFrames = 0;
    double fileRate = 44100.0, hostSampleRate = 44100.0;

    bool   enabled = false, keyMode = false, scheduling = true;
    double position = 0.0, spray = 0.1, sizeMs = 60.0, density = 20.0, noteHz = 261.6255653005986;
    double pitchSpread = 0.0, pitchCenter = 0.0, pitchFactor = 1.0, level = 0.5;
    int    quant = 0;
    double normExp = 0.5, lastNorm = 1.0;

    double   nextOnset = 0.0;
    uint32_t rng = 0x9E3779B9u;
    uint64_t spawned = 0, dropped = 0;
};
