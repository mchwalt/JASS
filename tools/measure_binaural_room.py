# Story 10.4 design measurement: binaural early-reflection stage (BinauralRoom.h).
# Pure stdlib (cmath + own FFT) per project rule "DSP messen statt raten".
#
# Verifies, BEFORE the C++ is written:
#   1. tap delays are mutually non-harmonic (no pitch ringing)
#   2. centre-transparency: 1/3-oct smoothed magnitude deviation at pan centre, room = 1
#   3. level neutrality of the constant-power normalisation (pink-weighted, room 0 / 0.5 / 1)
#   4. actual kernel energies (to base the normalisation constant on data, not on assumption)
import cmath, math, re, sys

HDR = r"d:\Projects\C++\JASS\Source\DSP\KemarHrir.h"
FS  = 44100.0
N   = 65536          # FFT size (~0.7 Hz resolution, enough for 8-25 ms combs)

# ---------- candidate design (mirrors the constants that will go into BinauralRoom.h) ----------
# Delays: primes in samples @44.1k => mutually non-harmonic by construction.
TAP_DELAY_SMP = [367, 499, 641, 773, 919, 1061]        # 8.3 / 11.3 / 14.5 / 17.5 / 20.8 / 24.1 ms
TAP_AZ_DEG    = [55, -40, 70, -60, 30, -50]            # lateral, alternating sides
TAP_GAIN      = [0.50, 0.45, 0.40, 0.36, 0.32, 0.29]   # decaying with distance/order
DAMP_HZ       = 5500.0                                  # one-pole LP on the reflection send (wall/air absorption)

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

# ---------- non-harmonicity check ----------
def check_nonharmonic(delays):
    worst = (0.0, None)
    for i in range(len(delays)):
        for k in range(i + 1, len(delays)):
            r = delays[k] / delays[i]
            near = abs(r - round(r))
            if round(r) >= 1 and near < worst[0] or worst[1] is None:
                pass
            # distance of ratio from the nearest integer (0 == perfectly harmonic)
            if worst[1] is None or near < worst[0]:
                worst = (near, (delays[i], delays[k], r))
    return worst

# ---------- build the bus impulse response at pan centre, per ear ----------
def send_impulse():
    """Impulse response of the damped reflection send (one-pole LP, wall/air absorption)."""
    a = 1.0 - math.exp(-2.0 * math.pi * DAMP_HZ / FS)
    send, y = [], 0.0
    for n in range(4096):
        y = a * (1.0 if n == 0 else 0.0) + (1.0 - a) * y
        send.append(y)
        if y < 1e-9 and n > 32: break
    return send

def wet_only_ir():
    """Per-ear IR of the reflection stage alone (unit wet gain) for a centred mono input
    (send == input at centre)."""
    length = max(TAP_DELAY_SMP) + TAPS + 4096
    irL = [0.0] * length
    irR = [0.0] * length
    send = send_impulse()
    for d, az, g in zip(TAP_DELAY_SMP, TAP_AZ_DEG, TAP_GAIN):
        kL, kR = kernels_for(az)
        for s, sv in enumerate(send):
            base = d + s
            if base >= length: break
            for t in range(min(TAPS, length - base)):
                irL[base + t] += g * sv * kL[t]
                irR[base + t] += g * sv * kR[t]
    return irL, irR

_P_cache = None
def reflection_power():
    """Empirical per-ear pink-power of the wet path relative to a unit dry Dirac.
    THIS is the constant the C++ normalisation will hard-code (includes kernels,
    damping and cross-tap incoherence — no assumptions)."""
    global _P_cache
    if _P_cache is None:
        irL, irR = wet_only_ir()
        ref = pink_power(mag_response([1.0]))
        _P_cache = (pink_power(mag_response(irL)) / ref,
                    pink_power(mag_response(irR)) / ref)
    return _P_cache

def room_ir(room):
    """Full stage IR at pan centre: dry Dirac + normalised reflections."""
    PL, PR = reflection_power()
    P = 0.5 * (PL + PR)
    dry_g = 1.0 / math.sqrt(1.0 + room * room * P)
    wet_g = room * dry_g
    wL, wR = wet_only_ir()
    irL = [wet_g * v for v in wL]
    irR = [wet_g * v for v in wR]
    irL[0] += dry_g; irR[0] += dry_g
    return irL, irR

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

def kernel_energy(az):
    kL, kR = kernels_for(az)
    mL, mR = mag_response(list(kL)), mag_response(list(kR))
    # pink-weighted POWER of the pair (dry equal-power pan pair would be 1.0 by the
    # generator's own normalisation) — this is the factor the normalisation must use.
    num = den = 0.0
    for i in range(1, N // 2):
        f = i * FS / N
        if 50.0 <= f <= 16000.0:
            w = 1.0 / f
            num += w * (mL[i] ** 2 + mR[i] ** 2)
            den += w
    return num / den

_kernel_pow = {}
def power_sum():
    P = 0.0
    for az, g in zip(TAP_AZ_DEG, TAP_GAIN):
        if az not in _kernel_pow: _kernel_pow[az] = kernel_energy(az)
        P += g * g * _kernel_pow[az]
    return P

def third_oct_smooth(mag):
    """1/3-octave smoothed magnitude (power average)."""
    out = [0.0] * len(mag)
    for i in range(1, len(mag)):
        f = i * FS / N
        lo, hi = f / (2 ** (1 / 6)), f * (2 ** (1 / 6))
        i0, i1 = max(1, int(lo * N / FS)), min(len(mag) - 1, int(hi * N / FS) + 1)
        s = 0.0
        for k in range(i0, i1 + 1): s += mag[k] * mag[k]
        out[i] = math.sqrt(s / (i1 - i0 + 1))
    return out

TARGET_P = 1.0    # wet == dry power at room=1 (user 2026-07-29: original 0.45 max was his
                  # preferred amount -> now sits at knob ~0.67 = the default, with headroom above)

def report_levels():
    ref = pink_level(mag_response([1.0]))
    for room in (0.0, 0.5, 1.0):
        irL, irR = room_ir(room)
        mL, mR = mag_response(irL), mag_response(irR)
        lvlL, lvlR = pink_level(mL), pink_level(mR)
        # smoothed deviation 100 Hz - 12 kHz vs the ear's own mean
        sm = third_oct_smooth(mL)
        devs = []
        for i in range(1, N // 2):
            f = i * FS / N
            if 100.0 <= f <= 12000.0 and sm[i] > 0:
                devs.append(20 * math.log10(sm[i]))
        mean = sum(devs) / len(devs)
        spread = max(devs) - min(devs)
        print(f"room={room:0.1f}: pink level L {lvlL - ref:+.2f} dB, R {lvlR - ref:+.2f} dB vs dry | "
              f"1/3-oct dev (100Hz-12kHz, L): spread {spread:.2f} dB, max {max(devs) - mean:+.2f}, min {min(devs) - mean:+.2f}")

def main():
    global TAP_GAIN, _P_cache
    near, pair = check_nonharmonic(TAP_DELAY_SMP)
    print(f"tap delays (smp): {TAP_DELAY_SMP}")
    print(f"tap delays (ms) : {[round(d / FS * 1000, 1) for d in TAP_DELAY_SMP]}")
    print(f"non-harmonicity : closest ratio-to-integer distance {near:.4f} (pair {pair[0]}/{pair[1]}, ratio {pair[2]:.3f})")
    PL, PR = reflection_power()
    P = 0.5 * (PL + PR)
    print(f"wet per-ear pink power (unit wet gain): L {PL:.4f}, R {PR:.4f}  (asymmetry {10*math.log10(PL/PR):+.2f} dB)")
    scale = math.sqrt(TARGET_P / P)
    print(f"gain rescale to hit target P={TARGET_P}: x{scale:.4f}")
    TAP_GAIN = [round(g * scale, 3) for g in TAP_GAIN]
    _P_cache = None
    PL, PR = reflection_power()
    P = 0.5 * (PL + PR)
    print(f"FINAL constants for BinauralRoom.h:")
    print(f"  kTapDelaysSmp44k = {TAP_DELAY_SMP}")
    print(f"  kTapAzimuthDeg   = {TAP_AZ_DEG}")
    print(f"  kTapGain         = {TAP_GAIN}")
    print(f"  kReflectionPower = {P:.4f}   (per-ear pink, measured; L {PL:.4f} / R {PR:.4f})")
    print(f"  kDampHz          = {DAMP_HZ}")
    report_levels()
    # octave-band profile at room=1 (L ear): smooth tilt = natural room; jagged = comb colour
    irL, _ = room_ir(1.0)
    mL = mag_response(irL)
    print("octave bands (L, room=1, dB re dry):")
    for fc in (63, 125, 250, 500, 1000, 2000, 4000, 8000, 12000):
        lo, hi = fc / math.sqrt(2), fc * math.sqrt(2)
        i0, i1 = max(1, int(lo * N / FS)), min(N // 2 - 1, int(hi * N / FS))
        p = sum(mL[i] * mL[i] for i in range(i0, i1 + 1)) / (i1 - i0 + 1)
        print(f"  {fc:>5} Hz: {10*math.log10(p):+.2f} dB")

if __name__ == "__main__":
    main()
