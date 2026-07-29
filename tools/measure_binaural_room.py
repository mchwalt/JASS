# Story 10.4 design measurement: binaural early-reflection stage (BinauralRoom.h).
# Pure stdlib (cmath + own FFT) per project rule "DSP messen statt raten".
#
# v2 (room MACRO): the knob is no longer a linear wet gain. Psychoacoustics (Zahorik 2002:
# direct-to-reverberant JND ~5-6 dB; Barron 1971 / precedence: spatial impression saturates
# within ~10 dB of the direct sound) made the linear knob feel dead above ~0.2. The knob now
# gangs two parameters:
#   wet level : -WET_RANGE_DB .. 0 dB relative to dry, exponential (hard off at knob 0)
#   damping   : one-pole LP on the send, 3.5 kHz .. 7 kHz (small dark room -> bigger brighter)
#
# Verifies:
#   1. tap delays are mutually non-harmonic (no pitch ringing)
#   2. wet-path pink power at BOTH damping endpoints (the C++ normalisation lerps between them)
#   3. level neutrality of the constant-power normalisation across the knob
#   4. centre-transparency: octave-band deviation at pan centre, knob = 1
import cmath, math, re

HDR = r"d:\Projects\C++\JASS\Source\DSP\KemarHrir.h"
FS  = 44100.0
N   = 65536          # FFT size (~0.7 Hz resolution, enough for 8-25 ms combs)

# ---------- design constants (mirror BinauralRoom.h) ----------
# Delays: primes in samples @44.1k => mutually non-harmonic by construction.
TAP_DELAY_SMP = [367, 499, 641, 773, 919, 1061]        # 8.3 / 11.3 / 14.5 / 17.5 / 20.8 / 24.1 ms
TAP_AZ_DEG    = [55, -40, 70, -60, 30, -50]            # lateral, alternating sides
TAP_GAIN      = [0.785, 0.707, 0.628, 0.565, 0.503, 0.456]   # P=1.0 tuning (wet==dry at full)
WET_RANGE_DB  = 24.0                                    # knob 0+ .. 1  ->  -24 .. 0 dB wet/dry
DAMP_LO_HZ    = 3500.0                                  # knob 0: small dark room
DAMP_HI_HZ    = 7000.0                                  # knob 1: bigger brighter room

def knob_wet(r):     return 0.0 if r <= 0.0 else 10.0 ** (-(WET_RANGE_DB * (1.0 - r)) / 20.0)
def knob_damp(r):    return DAMP_LO_HZ * 2.0 ** r       # exponential, one octave

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

def room_ir(r, p_lo, p_hi):
    """Full stage IR at pan centre for knob r, using the lerped normalisation the C++ uses."""
    w  = knob_wet(r)
    P  = p_lo + (p_hi - p_lo) * r
    dry = 1.0 / math.sqrt(1.0 + w * w * P)
    wet = w * dry
    wL, wR = wet_only_ir(knob_damp(r))
    irL = [wet * v for v in wL]
    irR = [wet * v for v in wR]
    irL[0] += dry; irR[0] += dry
    return irL, irR

def main():
    near, pair = check_nonharmonic(TAP_DELAY_SMP)
    print(f"tap delays (smp): {TAP_DELAY_SMP}  (ms: {[round(d / FS * 1000, 1) for d in TAP_DELAY_SMP]})")
    print(f"non-harmonicity : closest ratio-to-integer distance {near:.4f} (pair {pair[0]}/{pair[1]}, ratio {pair[2]:.3f})")

    PloL, PloR = reflection_power(DAMP_LO_HZ)
    PhiL, PhiR = reflection_power(DAMP_HI_HZ)
    p_lo = 0.5 * (PloL + PloR)
    p_hi = 0.5 * (PhiL + PhiR)
    print(f"CONSTANTS for BinauralRoom.h (per-ear pink wet power, unit wet gain):")
    print(f"  kWetPowerLo (damp {DAMP_LO_HZ:.0f} Hz) = {p_lo:.4f}   (L {PloL:.4f} / R {PloR:.4f})")
    print(f"  kWetPowerHi (damp {DAMP_HI_HZ:.0f} Hz) = {p_hi:.4f}   (L {PhiL:.4f} / R {PhiR:.4f})")

    ref = pink_level(mag_response([1.0]))
    print(f"knob sweep (wet dB rel. dry | damp | pink level vs dry L/R):")
    for r in (0.0, 0.25, 0.5, 0.75, 0.85, 1.0):
        irL, irR = room_ir(r, p_lo, p_hi)
        lvlL = pink_level(mag_response(irL)) - ref
        lvlR = pink_level(mag_response(irR)) - ref
        wdb = -999.0 if r <= 0 else 20 * math.log10(knob_wet(r))
        print(f"  r={r:0.2f}: wet {wdb:+7.1f} dB | damp {knob_damp(r):5.0f} Hz | level L {lvlL:+.2f} / R {lvlR:+.2f} dB")

    # centre colouration at full deflection (octave bands)
    irL, _ = room_ir(1.0, p_lo, p_hi)
    mL = mag_response(irL)
    print("octave bands (L, knob=1, dB re dry):")
    for fcb in (63, 125, 250, 500, 1000, 2000, 4000, 8000, 12000):
        lo, hi = fcb / math.sqrt(2), fcb * math.sqrt(2)
        i0, i1 = max(1, int(lo * N / FS)), min(N // 2 - 1, int(hi * N / FS))
        p = sum(mL[i] * mL[i] for i in range(i0, i1 + 1)) / (i1 - i0 + 1)
        print(f"  {fcb:>5} Hz: {10*math.log10(p):+.2f} dB")

if __name__ == "__main__":
    main()
