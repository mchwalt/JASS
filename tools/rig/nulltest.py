"""Null test for JASS refactors (Story 21.4).

Renders every preset through build/Release/JASS_Render.exe (one process per preset) and compares
two such renders sample for sample.

  python tools/rig/nulltest.py render  <JASS_Render.exe> <outdir> [preset.jass | folder ...]
  python tools/rig/nulltest.py compare <dirA> <dirB>

`render` without presets takes %AppData%/JASS/Presets/*.jass (the seeded demos included). Presets are
copied to <outdir>/_in first: PresetIO migrates an old-format file in place, and the user's files
must never be touched. Keep the reference exe OUTSIDE the build folder (copy it aside), or the
next build overwrites it.

`compare` exits 0 when every file is bit-identical; otherwise it lists max |diff| (dBFS) and the
first differing sample (seconds) per file.
"""
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]


def read_float_wav(path):
    data = Path(path).read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a WAV")
    pos, fmt, frames = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, sr, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE:                       # WAVE_FORMAT_EXTENSIBLE: real tag in the GUID
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, sr, bits)
        elif cid == b"data":
            frames = body
        pos += 8 + size + (size & 1)
    if fmt is None or frames is None or fmt[0] != 3 or fmt[3] != 32:
        raise ValueError(f"{path}: expected 32-bit float WAV, got {fmt}")
    return np.frombuffer(frames, dtype="<f4").reshape(-1, fmt[1]), fmt[2]


def collect_presets(args):
    if not args:
        # The AppData folder holds the seeded demos too (same names as DemoPresets/), so it alone
        # avoids two outputs fighting over one file name.
        args = [os.path.join(os.environ["APPDATA"], "JASS", "Presets")]
    out = []
    for a in args:
        p = Path(a)
        out += sorted(p.glob("*.jass")) if p.is_dir() else [p]
    return out


def render(exe, outdir, presets):
    exe = Path(exe).resolve()
    outdir = Path(outdir)
    indir = outdir / "_in"
    shutil.rmtree(indir, ignore_errors=True)
    indir.mkdir(parents=True)
    failed = []
    for p in collect_presets(presets):
        copy = indir / p.name
        shutil.copy2(p, copy)
        wav = outdir / (p.stem + ".wav")
        r = subprocess.run([str(exe), str(copy), str(wav)], capture_output=True, text=True, timeout=600)
        line = (r.stdout.strip() or r.stderr.strip()).splitlines()
        print(f"{p.name:40s} {'ok  ' if r.returncode == 0 else 'FAIL'} {line[-1] if line else ''}")
        if r.returncode != 0:
            failed.append(p.name)
    if failed:
        print(f"{len(failed)} preset(s) failed: {', '.join(failed)}")
    return 1 if failed else 0


def compare(a, b):
    a, b = Path(a), Path(b)
    names = sorted({p.name for p in a.glob("*.wav")} | {p.name for p in b.glob("*.wav")})
    bad = 0
    for n in names:
        pa, pb = a / n, b / n
        if not pa.exists() or not pb.exists():
            print(f"{n:40s} MISSING in {'A' if not pa.exists() else 'B'}")
            bad += 1
            continue
        xa, sr = read_float_wav(pa)
        xb, _ = read_float_wav(pb)
        if xa.shape != xb.shape:
            print(f"{n:40s} DIFFERENT LENGTH {xa.shape} vs {xb.shape}")
            bad += 1
            continue
        d = np.abs(xa.astype(np.float64) - xb.astype(np.float64))
        m = float(d.max()) if d.size else 0.0
        if m == 0.0 and np.array_equal(xa.view(np.uint32), xb.view(np.uint32)):
            print(f"{n:40s} identical")
            continue
        first = int(np.argmax(d.max(axis=1) > 0)) if m > 0 else 0
        db = 20 * np.log10(m) if m > 0 else float("-inf")
        print(f"{n:40s} DIFFERS  max {db:7.1f} dBFS, first at {first / sr:.4f} s (sample {first})")
        bad += 1
    print(f"{len(names) - bad} of {len(names)} identical")
    return 1 if bad else 0


if __name__ == "__main__":
    if len(sys.argv) >= 4 and sys.argv[1] == "render":
        sys.exit(render(sys.argv[2], sys.argv[3], sys.argv[4:]))
    if len(sys.argv) == 4 and sys.argv[1] == "compare":
        sys.exit(compare(sys.argv[2], sys.argv[3]))
    print(__doc__)
    sys.exit(2)
