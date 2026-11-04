"""18.6 host-clock test for JASS.vst3 in the SoundCraft rig (docs/notes/DAW_Candidates.md).

Builds a JUCE state blob (STEP SEQ on, 1/16, 16 steps all on, short gate), puts it on an
instrument track, drives the figure with one long clip note, bounces 4 bars headless and
measures every onset against the 1/16 grid.  Needs the SoundCraft fork built
(`cargo build --release` in D:/Projects/Rust/soundcraft) and the rig folder, see CLI/RIG below.

    python -I clocktest.py <name> <bpm> <note_start_beats> [--two-tracks] [--bounce-start=<s>]

  name              session and WAV name inside the rig folder
  bpm               session tempo (event.tempo)
  note_start_beats  where the clip note begins, in quarter notes (0 = session start)
  --two-tracks      two JASS instances with the same figure, bounced as stems and compared
  --bounce-start    bounce from this position in seconds instead of 0 (PLAY-anchor check)

Reported `dev` is the onset's distance from the grid counted from the clip note; with
--bounce-start the grid is counted from the session start, so a figure anchored at PLAY
(18.6's rule) shows a constant offset there - that is the expected result, not a drift.
"""
import sys, json, struct, base64, subprocess, wave, math, os
import numpy as np

CLI = r"D:\Projects\Rust\soundcraft\target\release\soundcraft-cli.exe"
RIG = r"D:\Projects\Rust\soundcraft-rig"
JASS = "vst3:01EFCDAB8291EBFA53796E7453796E31"
TPQ = 960

def state_blob(extra_params=None, pattern_steps=16):
    params = {"osc1On": 1.0, "osc2On": 0.0, "osc3On": 0.0, "seqOn": 1.0, "seqSync": 5.0,
              "seqLength": float(pattern_steps), "seqGate": 0.4,
              "attack": 0.001, "decay": 0.05, "sustain": 1.0, "release": 0.01}
    if extra_params: params.update(extra_params)
    xml = "<Parameters>" + "".join(f'<PARAM id="{k}" value="{v}"/>' for k, v in params.items())
    xml += f'<StepPattern on="{"X"*pattern_steps}" accent="{"."*pattern_steps}" pitch="{" ".join(["0"]*pattern_steps)}" gate="{" ".join(["100"]*pattern_steps)}"/>'
    xml += "</Parameters>"
    b = xml.encode("utf-8") + b"\0"
    blob = struct.pack("<II", 0x21324356, len(b)) + b
    return base64.b64encode(blob).decode()

def run(cmds, inp=None, save=None, bounce=None, start=None, end=None, inspect=False):
    args = [CLI, "run"]
    if inp: args += ["--in", inp]
    for c in cmds: args += ["--cmd", c]
    if save: args += ["--save", save]
    if bounce: args += ["--bounce", bounce]
    if start is not None: args += ["--start", str(start)]
    if end is not None: args += ["--end", str(end)]
    if inspect: args += ["--inspect", "--compact"]
    if inp and save and os.path.abspath(inp) == os.path.abspath(save): args += ["--in-place"]
    r = subprocess.run(args, capture_output=True, text=True, cwd=RIG)
    if r.returncode != 0:
        print("CLI FAILED", r.returncode, r.stdout[-2000:], r.stderr[-2000:]); sys.exit(1)
    return r.stdout

def read_wav(path):
    w = wave.open(path, "rb"); sr = w.getframerate(); ch = w.getnchannels(); sw = w.getsampwidth(); n = w.getnframes()
    raw = w.readframes(n); w.close()
    if sw == 3:
        a = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = (a[:, 0].astype(np.int32) | (a[:, 1].astype(np.int32) << 8) | (a[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float64) / (1 << 23)
    elif sw == 2:
        x = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    else:
        x = np.frombuffer(raw, dtype=np.float32).astype(np.float64)
    return sr, x.reshape(-1, ch).mean(axis=1)

def onsets(sr, x, thr_db=-40.0):
    """Sample-exact onsets: first sample above thr after >= 5 ms below thr/4."""
    thr = 10 ** (thr_db / 20); env = np.abs(x)
    above = env > thr
    minq = int(sr * 0.005); res = []; quiet = minq
    for i in range(len(x)):
        if above[i]:
            if quiet >= minq: res.append(i)
            quiet = 0
        elif env[i] < thr / 4:
            quiet += 1
    return res

def report(sr, x, bpm, note_start_beats, label):
    step = 60.0 / bpm / 4
    ons = onsets(sr, x)
    print(f"[{label}] sr={sr} len={len(x)/sr:.3f}s peak={20*math.log10(max(1e-9, np.abs(x).max())):.2f} dBFS onsets={len(ons)}")
    t0 = note_start_beats * 60.0 / bpm
    devs = []
    for i, s in enumerate(ons):
        t = s / sr
        k = round((t - t0) / step)
        dev_ms = (t - (t0 + k * step)) * 1000
        devs.append(dev_ms)
        if i < 6 or i >= len(ons) - 2:
            print(f"   onset {i:3d}: t={t:8.4f}s  step#{k:3d}  dev={dev_ms:+7.3f} ms ({(t-(t0+k*step))*sr:+6.1f} smp)")
    if devs:
        print(f"   dev ms: max|{max(abs(d) for d in devs):.3f}| mean {np.mean(devs):+.3f}  first {devs[0]:+.3f}  last {devs[-1]:+.3f}")
        gaps = np.diff(ons) / sr * 1000
        print(f"   gaps ms: min {gaps.min():.3f} max {gaps.max():.3f} expected {step*1000:.3f}")
    return ons

if __name__ == "__main__":
    name, bpm, nstart = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
    two = "--two-tracks" in sys.argv
    bstart = None
    for a in sys.argv:
        if a.startswith("--bounce-start="): bstart = float(a.split("=")[1])
    bars = 4
    sess = os.path.join(RIG, f"{name}.scraft")
    cmds = [f'event.tempo={{"bpm":{bpm}}}',
            f'track.new={{"kind":"instrument","name":"JASS","instrument":"{JASS}"}}',
            f'midi.new_clip={{"track":"JASS","start":0,"end":{int(48000*60/bpm*4*bars)}}}']
    if two:
        cmds += [f'track.new={{"kind":"instrument","name":"JASS2","instrument":"{JASS}"}}',
                 f'midi.new_clip={{"track":"JASS2","start":0,"end":{int(48000*60/bpm*4*bars)}}}']
    out = run(cmds, save=sess, inspect=True)
    lines = out.strip().splitlines()
    clips = [json.loads(l.split(": ",1)[1])["clip"] for l in lines if l.startswith("midi.new_clip:")]
    print("clips:", clips)
    st = int(nstart * TPQ)
    length = int((4 * bars) * TPQ) - st - 10
    cmds = [f'midi.note_add={{"clip":{clips[0]},"pitch":48,"start_ticks":{st},"length_ticks":{length}}}',
            f'mix.insert_state={{"track":"JASS","slot":"instrument","state":"{state_blob()}"}}']
    if two:
        cmds += [f'midi.note_add={{"clip":{clips[1]},"pitch":60,"start_ticks":{st},"length_ticks":{length}}}',
                 f'mix.insert_state={{"track":"JASS2","slot":"instrument","state":"{state_blob()}"}}']
    wav = os.path.join(RIG, f"{name}.wav")
    out = run(cmds, inp=sess, save=sess, bounce=wav, start=bstart)
    print(out.strip().splitlines()[-1])
    if not two:
        sr, x = read_wav(wav)
        report(sr, x, bpm, nstart - (bstart or 0) * bpm / 60.0, name)
    else:
        stems = os.path.join(RIG, name + "_stems")
        os.makedirs(stems, exist_ok=True)
        out = run([f'file.bounce_stems={{"dir":"{stems.replace(chr(92), "/")}"}}'], inp=sess)
        print(out.strip()[-300:])
        res = {}
        for f in sorted(os.listdir(stems)):
            if f.lower().endswith(".wav"):
                sr, x = read_wav(os.path.join(stems, f))
                res[f] = report(sr, x, bpm, nstart, f)
        keys = list(res)
        if len(keys) >= 2:
            a, b = np.array(res[keys[0]]), np.array(res[keys[1]])
            n = min(len(a), len(b))
            d = (a[:n] - b[:n]) / sr * 1000
            print(f"   phase diff {keys[0]} - {keys[1]}: max|{np.abs(d).max():.3f}| ms, counts {len(a)}/{len(b)}")
