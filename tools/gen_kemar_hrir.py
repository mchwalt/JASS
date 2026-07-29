#!/usr/bin/env python3
"""Generate Source/DSP/KemarHrir.h from the MIT KEMAR "compact" HRTF set (horizontal plane).

The MIT KEMAR compact set ships one stereo 44.1 kHz / 16-bit / 128-sample WAV per (elevation, azimuth):
its two channels are the LEFT-ear and RIGHT-ear impulse responses for a source at that azimuth, measured
to the RIGHT of the head (0..180 deg). The head is symmetric, so a source to the LEFT reuses |azimuth|
with the ears swapped. For JASS's left/right PAN we only need the FRONT hemisphere (0..90 deg), so this
tool reads elev0/H0e000a.wav .. H0e090a.wav (19 files, 5 deg steps) and emits a compact float header.

WHY THE RAW DATA CANNOT BE USED DIRECTLY (measured 2026-07-29). The frontal IR - the one that plays at
pan CENTRE, where no spatial effect is wanted at all - is itself a steep bandpass: -12 dB at 63 Hz
rising to +6 dB at 2 kHz, with a -15 dB notch at 8 kHz, 21.6 dB peak-to-peak. On headphones that is
heard as "buzzy, loses its warmth". Two of its three causes are not head-related information at all:

  * the missing bass is the MEASUREMENT RIG (MIT used a small Optimus Pro 7 speaker in 1994, which has
    no output down there), and
  * the 2-4 kHz rise and the 8 kHz notch are average pinna resonances, which the listener's OWN ears
    apply a second time over headphones - so they arrive twice.

What actually localises is the DEVIATION between the two ears and between azimuths, not this shared
colour. So the kernels are post-processed offline (the runtime cost is therefore zero), each one
rebuilt in a single FFT pass:

  1. FRONTAL ("free-field") EQUALISATION at full FFT resolution. The frontal response, 1/3-octave
     smoothed and limited to +/-MAX_CORR_DB, is divided out of every kernel - magnitude only, phase
     untouched, so the ITD survives. Referencing the FRONT (rather than the diffuse-field average over
     all azimuths) is what makes pan centre transparent: it becomes flat magnitude plus a pure delay,
     and panning then applies only the CHANGE relative to front. That is the property a musical panner
     needs; the mode becomes a spatial effect instead of a tone control.
  2. SYNTHESISED LOW END. Below LF_BLEND_LO the pinna imposes no spectral cue - the head is
     acoustically small and the cue is pure ITD - and a 128-tap kernel is far too short to carry real
     bass shaping anyway. But "flat magnitude with the correct delay" is just a delayed impulse, which
     IS compact. So the low end is replaced by exactly that: flat at the kernel's own midband level,
     with a linear phase set from that ear's delay measured at LF_ANCHOR_HZ (400 Hz - low enough that
     the interaural delay is already frequency-independent, high enough that the kernels still respond
     properly; measured at the blend corner itself the phase is ill-conditioned noise). Crossfaded
     into the measured response with a raised cosine over LF_BLEND_LO..LF_BLEND_HI.

     This replaced an earlier two-band design that routed the low band AROUND the convolution through
     a delay line. That looked equivalent but was not: the kernel's phase below ~300 Hz is not a
     meaningful delay at all (it measured -94 samples at 100 Hz), so the bypass could not be aligned
     against it, and the two paths partially cancelled - an 8.8 dB hole right at the crossover. Doing
     it inside the kernel has no second path to fight with, and it keeps the runtime a plain
     convolution.
  3. COMMON DELAY of KERNEL_SHIFT samples (a frequency-domain e^-jwS, i.e. a circular rotation). The
     equalisation rings BEFORE the impulse; without the rotation that pre-ring wraps to the far end of
     the FFT buffer and is discarded by the 128-tap window - unequally for the two ears, which is how
     a "phase-neutral" EQ still ends up bending the ITD. Rotating first captures it: 99.6 % of the
     energy lands inside the window instead of 96.1 %, and the worst ITD error drops from 66 to 23 us.
  4. PER-AZIMUTH LEVEL NORMALISATION, pink-weighted (equal energy per octave, the usual stand-in for
     musical spectra) rather than by raw sample energy. Raw energy is the mean of |H|^2 over a LINEAR
     frequency axis, where the top two octaves are three quarters of the range; matching it left the
     midrange - where music lives - about 1.4 dB low. Scaling each ear PAIR by one factor leaves the
     ILD untouched, and the target is the total power equal-power panning delivers, so Kunstkopf sits
     at the level of Stereo-Pan. (Note the parametric Binaural mode is itself ~3 dB hot: it drives
     both ears at unity at centre instead of 0.707. Stereo-Pan, not Binaural, is the reference here.)

Verified on the emitted tables: pan centre 4.2 dB peak-to-peak (from 21.6), level within 0.01 dB of
Stereo-Pan at every azimuth, worst ITD error 23 us, worst per-frequency ILD error 1.9 dB.

Data license (baked into the generated header): MIT KEMAR HRTF measurements, Bill Gardner & Keith
Martin, MIT Media Lab (1994) - free with no restrictions on use, provided the authors are cited.
  https://sound.media.mit.edu/resources/KEMAR.html

Usage (from the raw MIT data):
    python tools/gen_kemar_hrir.py --input <path-to>/compact/elev0 --output Source/DSP/KemarHrir.h
Download the raw data once from https://sound.media.mit.edu/resources/KEMAR/compact.zip (see tools/README.md).

Usage (re-process without re-downloading - reads the RAW values back out of an existing generated
header; refuses a header that was already post-processed, so the EQ can never be applied twice):
    python tools/gen_kemar_hrir.py --from-header Source/DSP/KemarHrir.h --output Source/DSP/KemarHrir.h
"""
import argparse, cmath, math, os, re, struct, wave

STEP_DEG = 5
MAX_AZ = 90          # front hemisphere only (L/R pan maps to +/-90 deg)
EXPECT_SR = 44100
EXPECT_TAPS = 128

# --- post-processing constants (see the module docstring for the reasoning behind each) -----------
FFT_N        = 512        # analysis/synthesis grid (the 128-tap IRs zero-padded)
SMOOTH_OCT   = 1.0 / 3.0  # frontal-reference smoothing: never invert a narrow notch into ringing
MAX_CORR_DB  = 12.0       # correction limit - beyond this we would amplify measurement noise
EQ_BAND_LO   = 300.0      # band the correction is referenced to (its mean becomes the 0 dB line)
EQ_BAND_HI   = 16000.0
LF_BLEND_LO  = 150.0      # below: fully synthesised low end
LF_BLEND_HI  = 500.0      # above: fully measured response
LF_ANCHOR_HZ = 400.0      # where each ear's low-frequency delay is measured
MIDBAND_LO   = 300.0      # band whose level the synthesised low end matches
MIDBAND_HI   = 4000.0
KERNEL_SHIFT = 8          # common circular delay, so the EQ's pre-ring lands inside the 128-tap window
NORM_LO      = 40.0       # pink-weighted normalisation band
NORM_HI      = 18000.0
# The guard in read_from_header() matches on this PREFIX, so the detail after it may vary freely.
MARKER_PREFIX = "POST-PROCESSING:"


# --- minimal FFT ---------------------------------------------------------------------------------

def fft(a):
    """Iterative radix-2 Cooley-Tukey, in-place on a list of complex. len(a) must be a power of 2."""
    n = len(a)
    assert n & (n - 1) == 0, "FFT length must be a power of two"
    j = 0
    for i in range(1, n):                      # bit-reversal permutation
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    length = 2
    while length <= n:
        step = cmath.exp(-2j * math.pi / length)
        for i in range(0, n, length):
            w = 1 + 0j
            for k in range(i, i + length // 2):
                u, v = a[k], a[k + length // 2] * w
                a[k], a[k + length // 2] = u + v, u - v
                w *= step
        length <<= 1
    return a


def ifft(a):
    n = len(a)
    conj = [x.conjugate() for x in a]
    fft(conj)
    return [x.conjugate() / n for x in conj]


def spectrum(h, n=FFT_N):
    return fft([complex(x) for x in h] + [0j] * (n - len(h)))


def bin_hz(k, sr=EXPECT_SR):
    return k * sr / FFT_N


def dft_at(h, f, sr=EXPECT_SR):
    """Single-frequency DFT - used for the log-spaced measurement grids."""
    w = 2.0 * math.pi * f / sr
    return sum(x * cmath.exp(-1j * w * n) for n, x in enumerate(h))


def log_grid(lo, hi, per_octave=6):
    out, f = [], lo
    step = 2.0 ** (1.0 / per_octave)
    while f < hi:
        out.append(f)
        f *= step
    return out


# --- input ---------------------------------------------------------------------------------------

def read_hrir(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 2, f"{path}: expected stereo"
        assert w.getframerate() == EXPECT_SR, f"{path}: expected {EXPECT_SR} Hz"
        assert w.getsampwidth() == 2, f"{path}: expected 16-bit"
        n = w.getnframes()
        raw = w.readframes(n)
    s = struct.unpack("<" + "h" * (n * 2), raw)   # interleaved L,R int16
    left = [s[i] / 32768.0 for i in range(0, len(s), 2)]
    right = [s[i] / 32768.0 for i in range(1, len(s), 2)]
    return left, right


def read_from_header(path):
    """Recover the RAW IR values from a previously generated header (int16/32768 round-trips
    exactly at the 8 decimals we emit). Refuses an already-post-processed header."""
    txt = open(path, encoding="utf-8", errors="replace").read()
    if MARKER_PREFIX in txt:
        raise SystemExit(
            f"{path} was already post-processed - re-running would apply the EQ twice.\n"
            f"Regenerate from the raw MIT data with --input instead (see tools/README.md)."
        )

    def rows(name):
        body = txt.split(f"float {name}[kNumAzimuths][kTaps] =", 1)[1].split("};", 1)[0]
        out = []
        for row in re.findall(r"\{([^{}]*)\}", body):
            vals = [float(v) for v in re.findall(r"(-?\d+\.\d+)f", row)]
            if vals:
                out.append(vals)
        return out

    return rows("kHrirLeft"), rows("kHrirRight")


# --- post-processing -----------------------------------------------------------------------------

def mean_log_mag_db(irs):
    """Mean log-magnitude spectrum over the given IRs, in dB, on the FFT grid."""
    half = FFT_N // 2 + 1
    acc = [0.0] * half
    for h in irs:
        X = spectrum(h)
        for k in range(half):
            acc[k] += 20.0 * math.log10(max(abs(X[k]), 1e-9))
    return [v / len(irs) for v in acc]


def smooth_octave(db, frac=SMOOTH_OCT):
    """Average in log-frequency bands of +/- frac/2 octave, so narrow notches are not inverted."""
    half = len(db)
    out = [0.0] * half
    lo_f = 2.0 ** (-frac / 2.0)
    hi_f = 2.0 ** (frac / 2.0)
    for k in range(half):
        if k == 0:
            out[0] = db[0]
            continue
        a = max(1, int(math.floor(k * lo_f)))
        b = min(half - 1, int(math.ceil(k * hi_f)))
        out[k] = sum(db[a:b + 1]) / (b - a + 1)
    return out


def reference_db(left_rows, right_rows, mode):
    """The response divided out of every kernel.

    'front' (default): the FRONTAL (az=0) response, which makes pan CENTRE transparent - see the
    module docstring. 'diffuse': the mean over all azimuths, more physically orthodox but it leaves
    the frontal response coloured, which is audible as a tone change when switching modes.
    """
    if mode == "front":
        return mean_log_mag_db([left_rows[0], right_rows[0]])
    return mean_log_mag_db(left_rows + right_rows)


def correction_gains(ref_db):
    """Per-bin linear magnitude scale that flattens the reference. Zero phase (a real, positive,
    frequency-domain multiplier), applied identically to both ears, hence ITD-neutral."""
    half = FFT_N // 2 + 1
    sm = smooth_octave(ref_db)
    band = [sm[k] for k in range(half) if EQ_BAND_LO <= bin_hz(k) <= EQ_BAND_HI]
    level = sum(band) / len(band)
    out = []
    for k in range(half):
        c = max(-MAX_CORR_DB, min(MAX_CORR_DB, -(sm[k] - level)))
        out.append(10.0 ** (c / 20.0))
    return out


def eq_spectrum(h, gains):
    half = FFT_N // 2 + 1
    X = spectrum(h)
    return [X[k] * gains[k] for k in range(half)]


def lf_delay(h):
    """That ear's low-frequency delay in samples, from the phase at LF_ANCHOR_HZ."""
    w = 2.0 * math.pi * LF_ANCHOR_HZ / EXPECT_SR
    acc = dft_at(h, LF_ANCHOR_HZ)
    return -cmath.phase(acc) / w if abs(acc) > 1e-12 else 0.0


def midband_level(Xc):
    """Pink-weighted RMS magnitude over MIDBAND_LO..MIDBAND_HI - the level the synthesised low end
    has to match so the crossfade does not step."""
    num = den = 0.0
    for k in range(len(Xc)):
        f = bin_hz(k)
        if MIDBAND_LO <= f <= MIDBAND_HI:
            num += abs(Xc[k]) ** 2 / f
            den += 1.0 / f
    return math.sqrt(num / den) if den > 0.0 else 0.0


def blend_weight(f):
    """0 below LF_BLEND_LO (synthesised), 1 above LF_BLEND_HI (measured), raised cosine between."""
    if f <= LF_BLEND_LO:
        return 0.0
    if f >= LF_BLEND_HI:
        return 1.0
    t = math.log(f / LF_BLEND_LO) / math.log(LF_BLEND_HI / LF_BLEND_LO)
    return 0.5 - 0.5 * math.cos(math.pi * t)


def build_kernel(h, gains, tau, m0):
    """One FFT pass: equalise, splice in the synthesised low end, rotate by KERNEL_SHIFT, window to
    EXPECT_TAPS. Returns (kernel, fraction of energy captured by the window)."""
    Xc = eq_spectrum(h, gains)
    half = FFT_N // 2 + 1
    Y = [0j] * FFT_N
    for k in range(half):
        f = bin_hz(k)
        w = 2.0 * math.pi * f / EXPECT_SR
        a = blend_weight(f)
        v = a * Xc[k] + (1.0 - a) * (m0 * cmath.exp(-1j * w * tau))
        Y[k] = v * cmath.exp(-1j * w * KERNEL_SHIFT)
        if 0 < k < FFT_N // 2:
            Y[FFT_N - k] = Y[k].conjugate()
    y = [v.real for v in ifft(Y)]
    kept = y[:EXPECT_TAPS]
    captured = sum(v * v for v in kept) / max(sum(v * v for v in y), 1e-20)
    for i in range(8):                                   # short fade-out: no hard cut at the tail
        kept[EXPECT_TAPS - 8 + i] *= (7 - i) / 8.0
    return kept, captured


def normalise_pair(hl, hr, grid):
    """Scale an ear PAIR by ONE factor (so the ILD is untouched) until its pink-weighted total power
    matches what equal-power panning delivers - i.e. the level of Stereo-Pan."""
    num = den = 0.0
    for f in grid:
        wt = 1.0 / f
        num += wt
        den += wt * (abs(dft_at(hl, f)) ** 2 + abs(dft_at(hr, f)) ** 2)
    if den <= 0.0:
        return hl, hr
    g = math.sqrt(num / den)
    return [x * g for x in hl], [x * g for x in hr]


# --- output --------------------------------------------------------------------------------------

def fmt_row(vals):
    return "        { " + ", ".join(f"{v:.8f}f" for v in vals) + " },"


def main():
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--input", help="path to the KEMAR compact/elev0 directory (raw MIT data)")
    src.add_argument("--from-header", help="re-read the raw values from an existing generated header")
    ap.add_argument("--output", required=True, help="output C++ header path")
    ap.add_argument("--raw", action="store_true",
                    help="skip all post-processing (emit the raw IRs). For A/B comparison only - the "
                         "result is the coloured, level-mismatched data described in the docstring.")
    ap.add_argument("--reference", choices=("front", "diffuse"), default="front",
                    help="response to equalise out: 'front' (default, pan centre becomes transparent) "
                         "or 'diffuse' (mean over all azimuths)")
    args = ap.parse_args()

    azimuths = list(range(0, MAX_AZ + 1, STEP_DEG))     # 0,5,..,90 -> 19

    if args.input:
        left_rows, right_rows, taps = [], [], None
        for az in azimuths:
            path = os.path.join(args.input, f"H0e{az:03d}a.wav")
            l, r = read_hrir(path)
            if taps is None:
                taps = len(l)
            assert len(l) == taps and len(r) == taps, f"{path}: tap-count mismatch"
            left_rows.append(l)
            right_rows.append(r)
    else:
        left_rows, right_rows = read_from_header(args.from_header)
        taps = len(left_rows[0])
        print(f"read raw IRs back from {args.from_header}")

    assert len(left_rows) == len(azimuths), f"expected {len(azimuths)} azimuths, got {len(left_rows)}"
    assert taps == EXPECT_TAPS, f"expected {EXPECT_TAPS} taps, got {taps}"

    if args.raw:
        print("--raw: emitting unprocessed IRs")
    else:
        gains = correction_gains(reference_db(left_rows, right_rows, args.reference))

        # Per-ear low-frequency delay, anchored where the kernels are well conditioned, then lifted
        # to be non-negative (one COMMON lift: every ITD is a difference, so it is unaffected).
        tauL = [lf_delay(h) for h in left_rows]
        tauR = [lf_delay(h) for h in right_rows]
        lift = -min(0.0, min(tauL), min(tauR))
        tauL = [t + lift for t in tauL]
        tauR = [t + lift for t in tauR]

        worst_capture = 1.0
        outL, outR = [], []
        for i in range(len(azimuths)):
            m0 = (midband_level(eq_spectrum(left_rows[i], gains))
                  + midband_level(eq_spectrum(right_rows[i], gains))) / 2.0
            kl, cl = build_kernel(left_rows[i], gains, tauL[i], m0)
            kr, cr = build_kernel(right_rows[i], gains, tauR[i], m0)
            worst_capture = min(worst_capture, cl, cr)
            outL.append(kl)
            outR.append(kr)

        grid = log_grid(NORM_LO, NORM_HI)
        for i in range(len(azimuths)):
            outL[i], outR[i] = normalise_pair(outL[i], outR[i], grid)
        left_rows, right_rows = outL, outR

        print(f"{args.reference}-reference EQ (full-resolution, 1/{1/SMOOTH_OCT:.0f}-oct smoothed, "
              f"<=+/-{MAX_CORR_DB:.0f} dB) + synthesised low end below {LF_BLEND_LO:.0f} Hz "
              f"+ pink-weighted level normalisation")
        print(f"  common delay {KERNEL_SHIFT} smp (+{lift:.2f} lift); "
              f"energy captured by the {EXPECT_TAPS}-tap window: worst {worst_capture*100:.2f} %")

    # ASCII-only content, written as UTF-8: the generated header is a build input, so it must not
    # depend on whatever code page the machine running this tool happens to use.
    with open(args.output, "w", encoding="utf-8", newline="\n") as f:
        f.write("#pragma once\n")
        f.write("// AUTO-GENERATED by tools/gen_kemar_hrir.py - do not edit by hand. Regenerate to change.\n")
        if not args.raw:
            f.write(f"// {MARKER_PREFIX} {args.reference}-reference EQ (full-resolution, "
                    f"1/{1/SMOOTH_OCT:.0f}-oct smoothed, <=+/-{MAX_CORR_DB:.0f} dB) + synthesised low "
                    f"end below {LF_BLEND_LO:.0f} Hz + pink-weighted level normalisation.  v2\n")
        f.write("//\n")
        f.write("// HRIR data: MIT KEMAR HRTF measurements (horizontal plane, compact set).\n")
        f.write("//   Bill Gardner & Keith Martin, MIT Media Lab (1994).\n")
        f.write("//   \"provided free with no restrictions on use, provided the authors are cited when the\n")
        f.write("//    data is used in any research or commercial application.\"\n")
        f.write("//   https://sound.media.mit.edu/resources/KEMAR.html\n")
        f.write("//\n")
        f.write("// Front hemisphere only (azimuths 0..90 deg, 5 deg steps): a left/right PAN maps to +/-90 deg,\n")
        f.write("// and the symmetric head mirrors the ears for the opposite side. kHrir{Left,Right}[az][tap] is\n")
        f.write("// the left-/right-ear impulse response for a source at that azimuth TO THE RIGHT of the head.\n")
        if not args.raw:
            f.write("//\n")
            f.write("// These kernels are NOT the raw MIT measurements. The raw frontal response is a 21.6 dB\n")
            f.write("// bandpass (no bass - the 1994 measurement speaker had none - plus doubled pinna resonances),\n")
            f.write("// which colours a CENTRED sound audibly even though no spatial effect is wanted there. So the\n")
            f.write("// frontal response is equalised out, the low end is replaced by a flat, correctly-delayed\n")
            f.write("// synthetic one (below a few hundred Hz the only cue is the interaural delay, and a 128-tap\n")
            f.write("// kernel cannot carry real bass shaping), and each pair is level-normalised to what\n")
            f.write("// equal-power panning delivers. What remains is the DIRECTIONAL deviation, which is what\n")
            f.write("// actually localises: pan centre is transparent, the ITD and the per-frequency ILD are intact,\n")
            f.write("// and the level matches the Stereo-Pan mode. See tools/gen_kemar_hrir.py for the reasoning\n")
            f.write("// and the verification figures. Convolve straight through - no crossover, no bypass path.\n")
        f.write("namespace KemarHrir\n{\n")
        f.write(f"    inline constexpr int kNumAzimuths   = {len(azimuths)};\n")
        f.write(f"    inline constexpr int kAzimuthStepDeg = {STEP_DEG};\n")
        f.write(f"    inline constexpr int kTaps          = {taps};\n")
        f.write(f"    inline constexpr int kSampleRate    = {EXPECT_SR};\n\n")
        for name, rows_ in (("kHrirLeft", left_rows), ("kHrirRight", right_rows)):
            f.write(f"    inline constexpr float {name}[kNumAzimuths][kTaps] =\n    {{\n")
            f.write("\n".join(fmt_row(r) for r in rows_))
            f.write("\n    };\n\n")
        f.write("}\n")

    print(f"wrote {args.output}: {len(azimuths)} azimuths x {taps} taps (L+R)")


if __name__ == "__main__":
    main()
