#!/usr/bin/env python3
"""Generate example wavetable .wav files for JASS.

Format expected by WavetableBank::loadFromWav: a mono WAV of concatenated
single-cycle FRAME-sample frames (each normalised per-frame on load). We write
FRAMES morphing single-cycle frames so the wavetable position knob sweeps the
timbre. Pure standard-library (wave + struct + math), no numpy needed.

Run from the repo root: python Wavetables/generate_wavetables.py
"""
import wave, struct, math, os

FRAME  = 2048          # samples per single cycle (WavetableBank::DefaultFrameSize)
FRAMES = 32            # morph steps across the table
SR     = 48000         # nominal; only the samples matter, not the pitch
OUTDIR = os.path.dirname(os.path.abspath(__file__))

def clamp(x): return max(-1.0, min(1.0, x))

def write_wav(name, frame_fn):
    frames_data = bytearray()
    for f in range(FRAMES):
        fn = f / (FRAMES - 1)                     # 0..1 morph position
        for i in range(FRAME):
            ph = i / FRAME                         # 0..1 phase within the cycle
            s = clamp(frame_fn(fn, ph))
            frames_data += struct.pack('<h', int(s * 32767))
    with wave.open(os.path.join(OUTDIR, name), 'w') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(bytes(frames_data))
    print("wrote", name, "-", FRAMES, "frames x", FRAME, "samples")

# --- basic single-cycle shapes -------------------------------------------------
def sine(ph):   return math.sin(2 * math.pi * ph)
def saw(ph):    return 2.0 * ph - 1.0
def square(ph): return 1.0 if ph < 0.5 else -1.0

# 1. Sine -> Saw: rounds out into a bright ramp as the position rises.
write_wav("SineToSaw.wav",    lambda fn, ph: (1 - fn) * sine(ph) + fn * saw(ph))

# 2. Saw -> Square: from buzzy ramp to hollow pulse.
write_wav("SawToSquare.wav",  lambda fn, ph: (1 - fn) * saw(ph)  + fn * square(ph))

# 3. Harmonic sweep: additive, adds harmonics 1..16 as the position rises.
def harmonic(fn, ph):
    n = 1 + int(fn * 15)                          # 1..16 partials
    return sum(math.sin(2 * math.pi * (k + 1) * ph) / (k + 1) for k in range(n))
write_wav("HarmonicSweep.wav", harmonic)

# 4. PWM sweep: pulse width narrows from 50% to ~5% across the table.
write_wav("PwmSweep.wav",     lambda fn, ph: 1.0 if ph < (0.5 - fn * 0.45) else -1.0)

# 5. Formant-ish: two shifting sine "peaks" (rough vowel morph).
def formant(fn, ph):
    f1 = 2 + fn * 6                               # first formant partial
    f2 = 8 + fn * 10                              # second formant partial
    return 0.6 * math.sin(2 * math.pi * f1 * ph) + 0.4 * math.sin(2 * math.pi * f2 * ph)
write_wav("FormantMorph.wav", formant)
