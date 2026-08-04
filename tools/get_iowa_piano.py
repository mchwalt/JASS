# Builds the optional "Piano" multisample set for JASS from the University of Iowa
# Electronic Music Studios "Musical Instrument Samples" (Steinway & Sons model B, ff layer).
# The recordings are freely available for any use without restrictions:
#   https://theremin.music.uiowa.edu/MIS.html
#
# What it does (all stdlib, no dependencies — Python 3.13+ safe, no `aifc`):
#   1. downloads 13 notes (tritone spacing C1..C7) as AIFF,
#   2. strips the leading room tone (~0.5 s — untrimmed edits make the set unplayable live),
#   3. trims to 10 s with a fade-out (fits comfortably inside JASS's per-set audio cap),
#   4. writes Piano_<note>.wav files following the JASS folder naming convention
#      (the _<note> suffix IS the key mapping; C4 = middle C).
#
# Usage:  python tools/get_iowa_piano.py [output-folder]
# Then copy/point the folder to %AppData%\JASS\Samples\Piano\ (or import it in the app
# via SAMPLER -> FOLDER).
import math, os, struct, sys, wave, array, urllib.request

NOTES  = ["C1", "Gb1", "C2", "Gb2", "C3", "Gb3", "C4", "Gb4", "C5", "Gb5", "C6", "Gb6", "C7"]
URL    = "https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/piano/Piano.ff.{}.aiff"
TRIM_S, FADE_S, ONSET_THRESH = 10.0, 1.5, 0.01

def read_aiff(data):
    assert data[:4] == b"FORM" and data[8:12] == b"AIFF", "not an AIFF"
    pos, ch, frames, bits, rate, pcm = 12, None, None, None, 44100.0, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos+4], struct.unpack_from(">I", data, pos+4)[0]
        body = data[pos+8:pos+8+size]
        if cid == b"COMM":
            ch, frames, bits = struct.unpack_from(">hLh", body, 0)
            exp  = struct.unpack_from(">H", body, 8)[0] & 0x7FFF   # 80-bit extended float
            mant = struct.unpack_from(">Q", body, 10)[0]
            rate = mant * 2.0 ** (exp - 16383 - 63)
        elif cid == b"SSND":
            off = struct.unpack_from(">I", body, 0)[0]
            pcm = body[8 + off:]
        pos += 8 + size + (size & 1)
    assert ch and frames and bits == 16 and pcm is not None, "unsupported AIFF layout"
    s = array.array("h")
    s.frombytes(pcm[: frames * ch * 2])
    if sys.byteorder == "little":
        s.byteswap()
    return ch, int(round(rate)), s

def convert(data, dst):
    ch, rate, s = read_aiff(data)
    thresh = int(ONSET_THRESH * 32767)                     # strip leading room tone
    nframes = len(s) // ch
    onset = next((i for i in range(nframes)
                  if any(abs(s[i * ch + c]) > thresh for c in range(ch))), 0)
    onset = max(0, onset - int(0.010 * rate))
    s = s[onset * ch:]
    fade_in = int(0.005 * rate)                            # click-free new start
    for i in range(min(fade_in, len(s) // ch)):
        for c in range(ch):
            s[i * ch + c] = int(s[i * ch + c] * i / fade_in)
    keep = min(len(s) // ch, int(TRIM_S * rate))
    fade = min(int(FADE_S * rate), keep)
    out = s[: keep * ch]
    for i in range(fade):                                  # linear fade-out at the trim point
        g = (fade - 1 - i) / fade
        for c in range(ch):
            out[(keep - fade + i) * ch + c] = int(out[(keep - fade + i) * ch + c] * g)
    with wave.open(dst, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(out.tobytes())
    print(f"  {os.path.basename(dst)}: {ch}ch {rate}Hz {keep/rate:.1f}s")

def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "Piano"
    os.makedirs(outdir, exist_ok=True)
    for note in NOTES:
        url = URL.format(note)
        print(f"downloading {note} ...")
        with urllib.request.urlopen(url, timeout=120) as r:
            data = r.read()
        convert(data, os.path.join(outdir, f"Piano_{note}.wav"))
    with open(os.path.join(outdir, "README.txt"), "w", encoding="utf-8") as f:
        f.write("Piano multisample set for JASS (13 zones, tritone spacing C1-C7, ff layer)\n\n"
                "Source: University of Iowa Electronic Music Studios - Musical Instrument\n"
                "        Samples (Steinway & Sons model B), freely available for use without\n"
                "        restrictions: https://theremin.music.uiowa.edu/MIS.html\n"
                "Processing: leading room tone stripped, trimmed to 10 s with fade-out,\n"
                "        renamed to the JASS folder naming convention (Piano_<note>.wav).\n"
                "Built by tools/get_iowa_piano.py from the JASS repository.\n")
    print(f"done -> {outdir}\\  (copy into %AppData%\\JASS\\Samples\\ or import via SAMPLER->FOLDER)")

if __name__ == "__main__":
    main()
