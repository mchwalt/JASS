# Regenerates the two synthesized samples of the LosNinosDrums kit
# (Samples/LosNinosDrums__LosNinosKick.wav, Samples/LosNinosDrums__DoorSlam.wav).
#
# Both are built from measurements of the record's percussion stem
# ("Los Ninos del Parque", Liaisons Dangereuses 1981; stem separated with
# lalal.ai, measured 2026-08-29 with the audio-measure workflow):
#
#   Kick: a Synare-style electronic kick. Pitch starts at ~105 Hz, holds
#   ~90 ms, then glides down to ~45 Hz over ~300 ms; the amplitude stays
#   within 1 dB of full level for ~90 ms ("drier" per the maintainer's ear;
#   the stem itself holds ~150 ms). Pure sine on purpose - a tanh-saturated
#   first take buzzed like a cajon.
#
#   DoorSlam: the backbeat "Tschak". Not a hi-hat: the extra energy on the
#   accented beats sits at 200-550 Hz with a broadband splash to ~6 kHz and
#   nothing above ~9 kHz, held ~75-100 ms and then gated. Modelled as three
#   band-split noise bursts (body 120-500 Hz longest, mid slap shorter,
#   highs only a 20 ms clack) plus a damped 190 Hz panel ping.
#
# Usage:  python tools/synth_losninos_kit.py
# Writes into the repo's Samples/ folder. Copy to %AppData%\JASS\Samples\
# LosNinosDrums\ (without the folder prefix) to hear it without a rebuild.

import os
import numpy as np
import wave

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "Samples")


def write_wav(name, y):
    y = y * 10 ** (-3 / 20) / np.abs(y).max()          # normalize to -3 dBFS
    fade = int(0.005 * SR)
    y[-fade:] *= np.linspace(1, 0, fade)               # declick tail
    path = os.path.join(OUT, name)
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes((y * 32767).astype(np.int16).tobytes())
    w.close()
    print("written", path)


def env(t_ms, pts_t, pts_db):
    return 10 ** (np.interp(t_ms, pts_t, pts_db) / 20)


def kick():
    n = int(SR * 0.35)
    t = np.arange(n) / SR * 1000.0
    # measured f0 trajectory (median over 100+ folded hits)
    f = np.interp(t, [0, 50, 100, 120, 150, 180, 200, 250, 300, 350],
                     [105, 105, 96,  83,  71,  60,  55,  49,  46,  44])
    # measured amplitude, plateau shortened to ~90 ms ("noch etwas trockener")
    amp = env(t, [0,   8,  20,   40, 90, 130, 180, 230, 280, 320, 350],
                 [-50, -4, -1.5, -0.5, 0, -3,  -9, -18, -30, -50, -70])
    return np.sin(np.cumsum(2 * np.pi * f / SR)) * amp


def door_slam():
    n = int(SR * 0.30)
    t = np.arange(n) / SR * 1000.0
    rng = np.random.default_rng(1981)                  # fixed seed = same file every run

    def shaped(flo, fhi):
        x = np.fft.rfft(rng.standard_normal(n))
        fr = np.fft.rfftfreq(n, 1 / SR)
        g = np.ones_like(fr)
        g[(fr < flo) | (fr > fhi)] = 0
        e = (fr >= flo) & (fr < flo * 1.3)
        g[e] = np.linspace(0, 1, e.sum())
        e = (fr > fhi / 1.3) & (fr <= fhi)
        g[e] = np.linspace(1, 0, e.sum())
        return np.fft.irfft(x * g, n)

    body = shaped(120, 500)  * env(t, [0, 4, 60, 120, 180, 240, 300], [-60, 0, -2, -8, -20, -40, -70])
    mid  = shaped(500, 2500) * env(t, [0, 2, 30, 60, 100, 160, 300],  [-50, 0, -3, -9, -20, -40, -70])
    clk  = shaped(2500, 8000) * env(t, [0, 1, 12, 25, 45, 300],       [-40, 0, -6, -20, -45, -80])
    ping = np.sin(2 * np.pi * 190 * t / 1000) * env(t, [0, 3, 80, 200, 300], [-60, -8, -16, -45, -80])
    return body + 0.45 * mid + 0.32 * clk + 0.5 * ping


write_wav("LosNinosDrums__LosNinosKick.wav", kick())
write_wav("LosNinosDrums__DoorSlam.wav", door_slam())
