# Story 10.4 design measurement: binaural early-reflection stage (BinauralRoom.h).
# Pure stdlib (cmath + own FFT) per project rule "DSP messen statt raten".
#
# v4 (5 DETENTS, perfect-in-the-middle): the knob is a 5-position macro. Psychoacoustics
# (Zahorik 2002: direct-to-reverberant JND ~5-6 dB; Barron 1971 / precedence: saturation within
# ~10 dB of the direct sound) killed the linear knob; the v2 -24..0 dB spread wasted the lower
# half below the user's effect threshold; v3 ended AT the ear-tested optimum. v4 (user request):
# the optimum sits in the MIDDLE of the travel and the upper half goes beyond it:
#   wet {off, -3, 0, +3, +6} dB  x  damping {-, 5, 7, 8.5, 10} kHz
# Detent 2 (knob 0.5, the default) is the ear-tested "perfect" (0 dB / 7 kHz) bit-exact.
# Each detent gets its own MEASURED normalisation constant (no interpolation).
#
# Verifies:
#   1. tap delays are mutually non-harmonic (no pitch ringing)
#   2. wet-path pink power PER DETENT (the C++ normalisation table)
#   3. level neutrality of the constant-power normalisation at every detent
#   4. centre-transparency: octave-band deviation at pan centre, top detent
import cmath, math, re

HDR = r"d:\Projects\C++\JASS\Source\DSP\KemarHrir.h"
FS  = 44100.0
N   = 65536          # FFT size (~0.7 Hz resolution, enough for 8-25 ms combs)

# ---------- design constants (mirror BinauralRoom.h) ----------
# Delays: primes in samples @44.1k => mutually non-harmonic by construction.
TAP_DELAY_SMP = [367, 499, 641, 773, 919, 1061]        # 8.3 / 11.3 / 14.5 / 17.5 / 20.8 / 24.1 ms
TAP_AZ_DEG    = [55, -40, 70, -60, 30, -50]            # lateral, alternating sides
TAP_GAIN      = [0.785, 0.707, 0.628, 0.565, 0.503, 0.456]   # P=1.0 tuning (wet==dry at top detent)
# Per-detent design (knob step 0.25 -> detent index 0..4). Detent 2 == the ear-tested "perfect".
DET_WET_DB    = [None, -3.0, 0.0, 3.0, 6.0]             # wet level rel. dry (None = off)
DET_DAMP_HZ   = [5000.0, 5000.0, 7000.0, 8500.0, 10000.0]

def det_wet(d):      return 0.0 if DET_WET_DB[d] is None else 10.0 ** (DET_WET_DB[d] / 20.0)

# ---------- parse the embedded kernels ----------
def parse_header(path):
    text = open(path, encoding="utf-8").read()
    tables = {}
    for name in ("kHrirLeft", "kHrirRight"):
        m = re.search(name + r"\[[^\]]*\]\[[^\]]*\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
        rows = re.findall(r"\{([^{}]*)\}", m.group(1))
        tables[name] = [[float(v.strip().rstrip("f")) for v in row.split(",")] for row in rows]
    return tables["kHrirLeft"], tables["kHrirRight"]

L, R = parse_header(HDR)
AZ_STEP, NAZ, TAPS = 5, len(L), len(L[0])

def kernels_for(az_deg):
    """(earL, earR) kernel pair for a source at az_deg (negative = left side => swap ears)."""
    idx = round(abs(az_deg) / AZ_STEP)
    return (L[idx], R[idx]) if az_deg >= 0 else (R[idx], L[idx])

# ---------- tiny FFT (iterative radix-2) ----------
def fft(x):
    n = len(x); x = list(x)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit: j ^= bit; bit >>= 1
        j |= bit
        if i < j: x[i], x[j] = x[j], x[i]
    length = 2
    while length <= n:
        ang = -2 * math.pi / length
        wl = cmath.exp(1j * ang)
        for i in range(0, n, length):
            w = 1 + 0j
            for k in range(i, i + length // 2):
                u, v = x[k], x[k + length // 2] * w
                x[k], x[k + length // 2] = u + v, u - v
                w *= wl
        length <<= 1
    return x

def mag_response(ir):
    X = fft(ir + [0.0] * (N - len(ir)))
    return [abs(v) for v in X[: N // 2]]

def pink_power(mag):
    """Pink-weighted mean power of a magnitude response, 50 Hz - 16 kHz."""
    num = den = 0.0
    for i in range(1, N // 2):
        f = i * FS / N
        if 50.0 <= f <= 16000.0:
            w = 1.0 / f
            num += w * mag[i] * mag[i]
            den += w
    return num / den

def pink_level(mag):
    return 10.0 * math.log10(pink_power(mag))

# ---------- non-harmonicity ----------
def check_nonharmonic(delays):
    worst = (999.0, None)
    for i in range(len(delays)):
        for k in range(i + 1, len(delays)):
            r = delays[k] / delays[i]
            near = abs(r - round(r))
            if near < worst[0]:
                worst = (near, (delays[i], delays[k], r))
    return worst

# ---------- wet path ----------
def send_impulse(fc):
    """Impulse response of the damped reflection send (one-pole LP)."""
    a = 1.0 - math.exp(-2.0 * math.pi * fc / FS)
    send, y = [], 0.0
    for n in range(4096):
        y = a * (1.0 if n == 0 else 0.0) + (1.0 - a) * y
        send.append(y)
        if y < 1e-9 and n > 32: break
    return send

_wet_cache = {}
def wet_only_ir(fc):
    """Per-ear IR of the reflection stage alone (unit wet gain), centred mono input."""
    if fc in _wet_cache: return _wet_cache[fc]
    length = max(TAP_DELAY_SMP) + TAPS + 4096
    irL = [0.0] * length
    irR = [0.0] * length
    send = send_impulse(fc)
    for d, az, g in zip(TAP_DELAY_SMP, TAP_AZ_DEG, TAP_GAIN):
        kL, kR = kernels_for(az)
        for s, sv in enumerate(send):
            base = d + s
            if base >= length: break
            for t in range(min(TAPS, length - base)):
                irL[base + t] += g * sv * kL[t]
                irR[base + t] += g * sv * kR[t]
    _wet_cache[fc] = (irL, irR)
    return _wet_cache[fc]

def reflection_power(fc):
    """Empirical per-ear pink power of the wet path (unit wet gain) vs a unit dry Dirac."""
    irL, irR = wet_only_ir(fc)
    ref = pink_power(mag_response([1.0]))
    return (pink_power(mag_response(irL)) / ref, pink_power(mag_response(irR)) / ref)

def room_ir(d, powers):
    """Full stage IR at pan centre for detent d, using the per-detent normalisation the C++ uses."""
    w  = det_wet(d)
    P  = powers[d]
    dry = 1.0 / math.sqrt(1.0 + w * w * P)
    wet = w * dry
    wL, wR = wet_only_ir(DET_DAMP_HZ[d])
    irL = [wet * v for v in wL]
    irR = [wet * v for v in wR]
    irL[0] += dry; irR[0] += dry
    return irL, irR

def main():
    near, pair = check_nonharmonic(TAP_DELAY_SMP)
    print(f"tap delays (smp): {TAP_DELAY_SMP}  (ms: {[round(d / FS * 1000, 1) for d in TAP_DELAY_SMP]})")
    print(f"non-harmonicity : closest ratio-to-integer distance {near:.4f} (pair {pair[0]}/{pair[1]}, ratio {pair[2]:.3f})")

    powers = [0.0] * 5
    print(f"CONSTANTS for BinauralRoom.h (kDetWetPower - per-ear pink wet power, unit wet gain):")
    for d in range(1, 5):
        PL, PR = reflection_power(DET_DAMP_HZ[d])
        powers[d] = 0.5 * (PL + PR)
        print(f"  detent {d} (damp {DET_DAMP_HZ[d]:5.0f} Hz): P = {powers[d]:.4f}   (L {PL:.4f} / R {PR:.4f})")

    ref = pink_level(mag_response([1.0]))
    print(f"detent sweep (wet dB rel. dry | damp | pink level vs dry L/R):")
    for d in range(5):
        irL, irR = room_ir(d, powers)
        lvlL = pink_level(mag_response(irL)) - ref
        lvlR = pink_level(mag_response(irR)) - ref
        wdb = "   off " if DET_WET_DB[d] is None else f"{DET_WET_DB[d]:+7.1f}"
        print(f"  detent {d}: wet {wdb} dB | damp {DET_DAMP_HZ[d]:5.0f} Hz | level L {lvlL:+.2f} / R {lvlR:+.2f} dB")

    # centre colouration at the default and the top detent (octave bands)
    for d in (2, 4):
        irL, _ = room_ir(d, powers)
        mL = mag_response(irL)
        print(f"octave bands (L, detent {d}, dB re dry):")
        for fcb in (63, 125, 250, 500, 1000, 2000, 4000, 8000, 12000):
            lo, hi = fcb / math.sqrt(2), fcb * math.sqrt(2)
            i0, i1 = max(1, int(lo * N / FS)), min(N // 2 - 1, int(hi * N / FS))
            p = sum(mL[i] * mL[i] for i in range(i0, i1 + 1)) / (i1 - i0 + 1)
            print(f"  {fcb:>5} Hz: {10*math.log10(p):+.2f} dB")

if __name__ == "__main__":
    main()
