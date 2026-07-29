# Generates the shipped SAMPLER example recordings (Story 12.1) - stdlib only (wave module).
# Small on purpose (embedded in the binary): three short 44.1k/16-bit files that showcase the
# module's dimensions - a mono one-shot, a STEREO seamless loop, and a reverse-friendly sweep.
# Filenames carry the root key (_C4) as a human hint; set ROOT=60 when playing them.
import math, wave, struct, random

FS = 44100

def write_wav(name, chans):
    n = len(chans[0])
    with wave.open(name, "wb") as w:
        w.setnchannels(len(chans))
        w.setsampwidth(2)
        w.setframerate(FS)
        frames = bytearray()
        for i in range(n):
            for c in chans:
                frames += struct.pack("<h", max(-32767, min(32767, int(c[i] * 32767))))
        w.writeframes(bytes(frames))
    print(f"{name}: {len(chans)}ch, {n/FS:.2f}s")

# 1) Bell_C4.wav - mono FM bell one-shot (~2.5 s), root C4.
def bell():
    f0, dur = 261.63, 2.5
    n = int(FS * dur)
    out = []
    for i in range(n):
        t = i / FS
        env = math.exp(-2.2 * t)
        mod = math.sin(2 * math.pi * f0 * 3.47 * t) * 2.5 * math.exp(-4.0 * t)
        s = math.sin(2 * math.pi * f0 * t + mod) * env
        s += 0.3 * math.sin(2 * math.pi * f0 * 2.0 * t) * math.exp(-3.0 * t)
        out.append(0.8 * s)
    return [out]

# 2) Texture_C4.wav - STEREO seamless pad loop (~4 s): detuned partial stack + slow stereo drift.
#    Built from components periodic in the loop length => mathematically seamless (Loop mode).
def texture():
    dur = 4.0
    n = int(FS * dur)
    f0 = 130.81  # C3
    partials = [(1.0, 0.5), (1.5, 0.25), (2.0, 0.3), (2.997, 0.15), (4.0, 0.12), (5.04, 0.08)]
    L, R = [], []
    for i in range(n):
        t = i / FS
        drift = math.sin(2 * math.pi * t / dur)          # loop-periodic drift
        l = r = 0.0
        for k, (ratio, amp) in enumerate(partials):
            ph = 2 * math.pi * f0 * ratio * t
            a = amp * (1.0 + 0.3 * math.sin(2 * math.pi * (k + 1) * t / dur))   # loop-periodic AM
            l += a * math.sin(ph + 0.5 * k)
            r += a * math.sin(ph + 0.5 * k + 0.6 * drift)                        # phase drift = width
        L.append(0.35 * l)
        R.append(0.35 * r)
    return [L, R]

# 3) Sweep_C4.wav - mono riser (~2 s): filtered-noise-ish sweep, fun in Reverse/Rev-Loop.
def sweep():
    dur = 2.0
    n = int(FS * dur)
    rng = random.Random(1234)
    out, lp = [], 0.0
    for i in range(n):
        t = i / FS
        fc = 200.0 * (2.0 ** (t / dur * 5.0))            # 200 Hz -> 6.4 kHz
        alpha = 1.0 - math.exp(-2 * math.pi * fc / FS)
        lp += alpha * ((rng.random() * 2 - 1) - lp)
        env = min(t / 0.02, 1.0) * (1.0 - math.exp(-(dur - t) * 8.0) if t > dur - 0.4 else 1.0)
        tone = 0.25 * math.sin(2 * math.pi * 261.63 * t * (1.0 + 0.5 * t / dur))
        out.append(0.8 * (0.7 * lp + tone) * env)
    return [out]

write_wav("Bell_C4.wav", bell())
write_wav("Texture_C4.wav", texture())
write_wav("Sweep_C4.wav", sweep())
