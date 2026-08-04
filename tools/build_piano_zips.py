#!/usr/bin/env python3
"""Build the redistributable JASS grand-piano zips (release assets, NOT repo content).

The curated .sfz files live in tools/piano-packs/ (a few KB — the versioned part);
the samples are fetched from the upstream GitHub repos and are NOT committed here
(~65 MB — they go into a dedicated GitHub release, e.g. tag `piano-pack-v1`).

    python tools/build_piano_zips.py [--src-splendid DIR] [--src-salamander DIR] [--out DIR]

Without --src-* the upstream repos are shallow-cloned into a temp folder (needs git
+ network). The zips unzip straight into %AppData%\\JASS\\Samples\\ :

    JASS-SplendidPiano.zip    -> SplendidPiano/SplendidPiano.sfz + Samples/*.flac + NOTICE.txt
    JASS-SalamanderPiano.zip  -> SalamanderPiano/... + ATTRIBUTION.txt (CC BY 3.0)

Licensing: Splendid samples are Public Domain (AKAI); upstream asks derived sfz
mappings to note their origin — NOTICE.txt does. Salamander is CC BY 3.0 —
ATTRIBUTION.txt carries author, source, license link and the change list.
"""
import argparse, re, shutil, subprocess, sys, tempfile, zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

PACKS = [
    {
        "name": "SplendidPiano",
        "upstream": "https://github.com/sfzinstruments/SplendidGrandPiano",
        "notice_file": "NOTICE.txt",
        "notice": """Splendid Grand Piano — JASS edition
====================================

Samples: Public Domain, released by AKAI (Steinway recordings).

Derived from the SFZ instrument at
  https://github.com/sfzinstruments/SplendidGrandPiano
(mapping by kinwie; string resonance / half-pedalling by Peter Eastman —
those features are not part of this subset).

Changes for JASS (https://github.com/mchwalt/JASS):
  - flattened, single velocity layer (FF; Mf in the undampened top range),
    minimal opcode subset readable by the JASS SAMPLER
  - per-region release times kept from upstream data
  - keys A3/A4 (MIDI 57/69) keep their original FF recordings, which carry a
    faint ~2 kHz ring from the source material (documented, accepted)

Install: unzip into %AppData%\\JASS\\Samples\\  (the SplendidPiano folder),
or load SplendidPiano.sfz via SAMPLER -> LOAD.
""",
    },
    {
        "name": "SalamanderPiano",
        "upstream": "https://github.com/sfzinstruments/SalamanderGrandPiano",
        "notice_file": "ATTRIBUTION.txt",
        "notice": """Salamander Grand Piano V3 — JASS edition
=========================================

"Salamander Grand Piano" (Yamaha C5) by Alexander Holm,
licensed under CC BY 3.0: https://creativecommons.org/licenses/by/3.0/
Source: https://github.com/sfzinstruments/SalamanderGrandPiano
(retuned version by Markus Fiedler, SFZ reconstruction by kinwie).

Changes for JASS (https://github.com/mchwalt/JASS):
  - single velocity layer (v16) of the original 16
  - flattened, minimal-opcode .sfz readable by the JASS SAMPLER
  - fixed per-zone release times (1.0 s damped, 3.0 s undampened range)
  - hammer noises, pedal noises, release samples and string resonance omitted

Install: unzip into %AppData%\\JASS\\Samples\\  (the SalamanderPiano folder),
or load SalamanderPiano.sfz via SAMPLER -> LOAD.
""",
    },
]


def referenced_samples(sfz: Path) -> list[str]:
    """sample= values from the curated sfz (comments stripped; values may hold spaces)."""
    names = []
    for line in sfz.read_text(encoding="utf-8").splitlines():
        line = line.split("//")[0]
        m = re.search(r"sample=(.+?)(?:\s+\w+=|$)", line)
        if m:
            names.append(m.group(1).strip())
    return names


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src-splendid")
    ap.add_argument("--src-salamander")
    ap.add_argument("--out", default=str(REPO / "build" / "piano-packs"))
    args = ap.parse_args()
    src_override = {"SplendidPiano": args.src_splendid, "SalamanderPiano": args.src_salamander}

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for pack in PACKS:
            name = pack["name"]
            src = src_override[name]
            if src:
                src = Path(src)
            else:
                src = Path(tmp) / name
                print(f"cloning {pack['upstream']} (shallow) ...")
                subprocess.run(["git", "clone", "--depth", "1", pack["upstream"], str(src)],
                               check=True)
            sample_dir = src / "Samples"
            sfz = REPO / "tools" / "piano-packs" / f"{name}.sfz"
            files = referenced_samples(sfz)
            missing = [f for f in files if not (sample_dir / f).is_file()]
            if missing:
                print(f"ERROR: {name}: missing upstream samples: {missing}", file=sys.stderr)
                return 1
            zpath = out / f"JASS-{name}.zip"
            with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
                z.writestr(f"{name}/{pack['notice_file']}", pack["notice"])
                z.write(sfz, f"{name}/{name}.sfz")
                for f in files:   # FLAC is already compressed — DEFLATE just dedups headers
                    z.write(sample_dir / f, f"{name}/Samples/{f}")
            print(f"{zpath}  ({zpath.stat().st_size / 1e6:.1f} MB, {len(files)} samples)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
