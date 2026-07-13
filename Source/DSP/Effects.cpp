#include "Effects.h"

// --- Distortion ---

static double distortionFoldback(double x)
{
    // Fold signal back when it exceeds ±1
    while (x > 1.0 || x < -1.0)
    {
        if (x > 1.0) x = 2.0 - x;
        if (x < -1.0) x = -2.0 - x;
    }
    return x;
}

float DistortionEffect::process(float input)
{
    if (type == DistortionType::Off) return input;

    double gain = 1.0 + drive * 19.0; // 1x to 20x
    double driven = input * gain;

    double distorted;
    switch (type)
    {
        case DistortionType::SoftClip: distorted = std::tanh(driven); break;
        case DistortionType::HardClip: distorted = std::clamp(driven, -1.0, 1.0); break;
        case DistortionType::Foldback: distorted = distortionFoldback(driven); break;
        default:                       distorted = driven; break;
    }

    double output = input * (1.0 - mix) + distorted * mix;
    return static_cast<float>(std::clamp(output, -1.0, 1.0));
}

// --- Wavefolder ---

float WavefolderEffect::process(float input)
{
    if (!enabled) return input;

    constexpr double PI = 3.14159265358979323846;

    double g = 1.0 + drive * 7.0;  // 1x..8x fold gain — more gain = more folds
    // Asymmetric gain per half-cycle adds even harmonics while keeping f(0)=0
    // (so no DC offset is introduced).
    double asym = (input >= 0.0f) ? (1.0 + symmetry) : (1.0 - symmetry);
    double folded = std::sin(static_cast<double>(input) * g * asym * PI * 0.5);

    double output = input * (1.0 - mix) + folded * mix;
    return static_cast<float>(std::clamp(output, -1.0, 1.0));
}

// --- Bitcrusher ---

float BitcrusherEffect::process(float input)
{
    if (!enabled) return input;

    // Sample-rate reduction: hold one value for 'rate' samples (sample & hold).
    int hold = std::max(1, (int) rate);
    if (counter == 0)
        held = input;
    counter = (counter + 1) % hold;
    double s = held;

    // Bit-depth reduction: quantise to 2^bits steps across the -1..1 range.
    double levels = std::pow(2.0, std::clamp(bits, 1.0, 16.0));
    double step = 2.0 / levels;
    s = step * std::floor(s / step + 0.5);

    double output = input * (1.0 - mix) + s * mix;
    return static_cast<float>(std::clamp(output, -1.0, 1.0));
}

// --- Delay ---

void DelayEffect::prepare(double sampleRate)
{
    sr = sampleRate;
    // 6.5 s covers the longest tempo-synced division (1/1 down to 40 BPM = 6.0 s) plus the
    // 2 s free-knob range; process() clamps the read offset to the buffer, so it never overruns.
    buffer.assign(static_cast<int>(6.5 * sr), 0.0f);
    writePos = 0;
}

float DelayEffect::process(float input)
{
    if (!enabled || buffer.empty()) return input;

    int bufSize = static_cast<int>(buffer.size());
    int delaySamples = std::clamp(static_cast<int>(time * sr), 1, bufSize - 1);
    int readPos = (writePos - delaySamples + bufSize) % bufSize;

    float delayed = buffer[readPos];
    buffer[writePos] = input + delayed * static_cast<float>(feedback);
    writePos = (writePos + 1) % bufSize;

    return input * (1.0f - static_cast<float>(mix)) + delayed * static_cast<float>(mix);
}

void DelayEffect::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
}

// --- Phaser / Flanger ---

void PhaserEffect::prepare(double sampleRate)
{
    sr = sampleRate;
    buffer.assign(static_cast<int>(0.02 * sr), 0.0f);   // up to 20 ms for the flanger delay line
    reset();
}

float PhaserEffect::process(float input)
{
    if (!enabled) return input;

    // Shared bipolar LFO, mapped to 0..1.
    const double lfo = std::sin(2.0 * M_PI * lfoPhase);
    lfoPhase += rate / sr;
    if (lfoPhase >= 1.0) lfoPhase -= 1.0;
    const double mod = lfo * 0.5 + 0.5;   // 0..1

    if (type == PhaserType::Phaser)
    {
        // Sweep the all-pass corner frequency (logarithmically) over the depth range;
        // deeper depth = wider sweep. fb feeds the chain output back into its input.
        const double fc  = 200.0 * std::pow(2.0, mod * (1.0 + depth * 4.0));   // ~200 Hz .. a few kHz
        const double t   = std::tan(M_PI * std::clamp(fc, 20.0, sr * 0.45) / sr);
        const double g   = (t - 1.0) / (t + 1.0);                              // first-order all-pass coeff (-1..1)

        float x = input + static_cast<float>(feedback) * lastPhaser;
        for (int s = 0; s < kStages; ++s)
        {
            const float y = static_cast<float>(-g) * x + apState[s];
            apState[s]    = x + static_cast<float>(g) * y;
            x = y;
        }
        lastPhaser = x;
        return input * (1.0f - static_cast<float>(mix)) + x * static_cast<float>(mix);
    }

    // Flanger: short swept delay (1..7 ms) with feedback → moving comb filter.
    if (buffer.empty()) return input;
    const int bufSize = static_cast<int>(buffer.size());

    const double delaySec     = 0.001 + depth * 0.006 * mod;   // 1 ms .. up to 7 ms
    const double delaySamples = delaySec * sr;
    const int    r1   = (static_cast<int>(writePos - delaySamples) % bufSize + bufSize) % bufSize;
    const int    r2   = (r1 + 1) % bufSize;
    const double frac = delaySamples - std::floor(delaySamples);
    const float  delayed = static_cast<float>(buffer[r1] * (1.0 - frac) + buffer[r2] * frac);

    buffer[writePos] = input + static_cast<float>(feedback) * delayed;
    writePos = (writePos + 1) % bufSize;
    lastFlanger = delayed;
    return input * (1.0f - static_cast<float>(mix)) + delayed * static_cast<float>(mix);
}

void PhaserEffect::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    for (auto& s : apState) s = 0.0f;
    writePos = 0;
    lfoPhase = 0.0;
    lastPhaser = 0.0f;
    lastFlanger = 0.0f;
}

// --- Formant / vowel filter ---

void FormantFilter::BandPass::set(double freq, double q, double sr)
{
    // RBJ band-pass (constant 0 dB peak gain).
    const double w0    = 2.0 * M_PI * std::clamp(freq, 20.0, sr * 0.45) / sr;
    const double cw    = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * std::max(0.5, q));
    const double a0    = 1.0 + alpha;
    b0 =  alpha / a0;
    b1 =  0.0;
    b2 = -alpha / a0;
    a1 = (-2.0 * cw) / a0;
    a2 = (1.0 - alpha) / a0;
}

float FormantFilter::BandPass::process(float x)
{
    // Transposed Direct Form II.
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return static_cast<float>(y);
}

void FormantFilter::prepare(double sampleRate)
{
    sr = sampleRate;
    reset();
    lastVowel = lastReso = -1.0;   // force a coefficient recompute on the first process()
}

void FormantFilter::updateCoeffs()
{
    // Vowel formant centre frequencies (Hz): F1, F2, F3 for A, E, I, O, U.
    static const double F[5][3] = {
        { 800.0, 1150.0, 2900.0 },  // A
        { 400.0, 1600.0, 2700.0 },  // E
        { 350.0, 1700.0, 2700.0 },  // I
        { 450.0,  800.0, 2830.0 },  // O
        { 325.0,  700.0, 2530.0 },  // U
    };
    const double pos = std::clamp(vowel, 0.0, 1.0) * 4.0;   // 0..4 across the five vowels
    const int    i0  = std::min(3, (int) pos);
    const int    i1  = i0 + 1;
    const double fr  = pos - i0;
    const double q   = 2.0 + std::clamp(resonance, 0.0, 1.0) * 18.0;   // Q 2..20
    for (int b = 0; b < 3; ++b)
        bands[b].set(F[i0][b] * (1.0 - fr) + F[i1][b] * fr, q, sr);
}

float FormantFilter::process(float input)
{
    if (!enabled) return input;
    if (vowel != lastVowel || resonance != lastReso)
    {
        updateCoeffs();
        lastVowel = vowel;
        lastReso  = resonance;
    }
    // F1 loudest, F2/F3 progressively quieter (rough formant amplitudes).
    const float wet = bands[0].process(input) * 1.0f
                    + bands[1].process(input) * 0.6f
                    + bands[2].process(input) * 0.35f;
    return input * (1.0f - static_cast<float>(mix)) + wet * static_cast<float>(mix);
}

void FormantFilter::reset()
{
    for (auto& b : bands) b.reset();
}

// --- Chorus ---

void ChorusEffect::prepare(double sampleRate)
{
    sr = sampleRate;
    buffer.assign(static_cast<int>(0.05 * sr), 0.0f);
    writePos = 0;
    lfoPhase = 0.0;
}

float ChorusEffect::process(float input)
{
    if (!enabled || buffer.empty()) return input;

    int bufSize = static_cast<int>(buffer.size());
    buffer[writePos] = input;

    double lfo = std::sin(2.0 * M_PI * lfoPhase);
    lfoPhase += rate / sr;
    if (lfoPhase >= 1.0) lfoPhase -= 1.0;

    double delaySec = depth * (1.0 + lfo) * 0.5 + 0.001;
    double delaySamples = delaySec * sr;

    int readPos1 = static_cast<int>(writePos - delaySamples + bufSize * 2) % bufSize;
    int readPos2 = (readPos1 + 1) % bufSize;
    double frac = delaySamples - static_cast<int>(delaySamples);

    float delayed = static_cast<float>(buffer[readPos1] * (1.0 - frac) + buffer[readPos2] * frac);
    writePos = (writePos + 1) % bufSize;

    return input * (1.0f - static_cast<float>(mix)) + delayed * static_cast<float>(mix);
}

void ChorusEffect::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    lfoPhase = 0.0;
}

// --- Reverb ---

float ReverbEffect::CombFilter::process(float input, float fb, float damp)
{
    float output = buffer[pos];
    filterStore = output * (1.0f - damp) + filterStore * damp;
    buffer[pos] = input + filterStore * fb;
    pos = (pos + 1) % static_cast<int>(buffer.size());
    return output;
}

float ReverbEffect::AllpassFilter::process(float input)
{
    float delayed = buffer[pos];
    float output = -input + delayed;
    buffer[pos] = input + delayed * 0.5f;
    pos = (pos + 1) % static_cast<int>(buffer.size());
    return output;
}

void ReverbEffect::prepare(double sampleRate)
{
    double scale = sampleRate / 44100.0;
    combs[0].init(static_cast<int>(1116 * scale));
    combs[1].init(static_cast<int>(1188 * scale));
    combs[2].init(static_cast<int>(1277 * scale));
    combs[3].init(static_cast<int>(1356 * scale));
    allpasses[0].init(static_cast<int>(556 * scale));
    allpasses[1].init(static_cast<int>(441 * scale));
    initialized = true;
}

float ReverbEffect::process(float input)
{
    if (!enabled || !initialized) return input;

    float fb = static_cast<float>(roomSize * 0.9 + 0.1);
    float damp = static_cast<float>(damping);

    float combOut = 0.0f;
    for (auto& comb : combs)
        combOut += comb.process(input, fb, damp);
    combOut *= 0.25f;

    float output = combOut;
    for (auto& ap : allpasses)
        output = ap.process(output);

    return input * (1.0f - static_cast<float>(mix)) + output * static_cast<float>(mix);
}

void ReverbEffect::reset()
{
    initialized = false;
}
