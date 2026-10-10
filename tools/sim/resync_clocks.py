# Faithful Python port of StepSequencer / PercSequencer clocks + the processor PERC transport block.
# Checks that a STOP/release re-sync lands figure step 0 and drum step 0 on the same sample
# (found the drum-clock drift of 2026-10-11). Run: python -I tools/sim/resync_clocks.py
# STOP/release re-sync lands figure step 0 and drum step 0 on the same sample.
import random

class Seq:
    def __init__(self, interval, steps):
        self.interval, self.steps = interval, steps
        self.sampleCounter = 0; self.stepIndex = 0; self.litStep = -1
        self.startDelay = 0; self.pendingRestart = -1
        self.enabled = True
        self.fired = []   # (abs_sample, step)
    def samplesToNextStep(self):
        return 0 if self.sampleCounter == 0 else self.interval - self.sampleCounter
    def restartLegatoIn(self, n): self.pendingRestart = max(0, n)
    def process(self, n, t0):
        for i in range(n):
            if self.startDelay > 0:
                self.startDelay -= 1; continue
            if self.pendingRestart == 0:
                self.pendingRestart = -1; self.stepIndex = 0; self.sampleCounter = 0
            elif self.pendingRestart > 0:
                self.pendingRestart -= 1
            if self.sampleCounter == 0:
                s = self.stepIndex % self.steps; self.litStep = s
                self.fired.append((t0 + i, s))
                self.stepIndex = (self.stepIndex + 1) % self.steps
            self.sampleCounter += 1
            if self.sampleCounter >= self.interval: self.sampleCounter = 0

class Perc:
    def __init__(self, interval, steps):
        self.interval, self.steps = interval, steps
        self.sampleCounter = 0; self.stepIndex = 0; self.litStep = -1; self.startDelay = 0
        self.enabled = False
        self.fired = []
    def reset(self):
        self.sampleCounter = 0; self.stepIndex = 0; self.litStep = -1; self.startDelay = 0
    def setStartDelay(self, n): self.startDelay = max(0, n)
    def process(self, n, t0):
        for i in range(n):
            if self.startDelay > 0:
                self.startDelay -= 1
            else:
                if self.enabled and self.sampleCounter == 0:
                    s = self.stepIndex % self.steps; self.litStep = s
                    self.fired.append((t0 + i, s))
                    self.stepIndex = (self.stepIndex + 1) % self.steps
                self.sampleCounter += 1
                if self.sampleCounter >= self.interval: self.sampleCounter = 0

def run(seed):
    rnd = random.Random(seed)
    interval = 8448   # 1/8 at 156 BPM, 44.1 kHz
    seq, perc = Seq(interval, 16), Perc(interval, 16)
    block = rnd.choice([64, 128, 256, 480, 512, 1024])
    held = False
    t = 0
    # schedule: run both from t=0 (both start aligned), STOP at tA, release at tB
    tA = rnd.randrange(interval * 3, interval * 20)
    tB = tA + rnd.randrange(100, interval * 9)
    while t < tB + interval * 40:
        percOn = (t < tA) or (t >= tB)   # held between tA and tB (block granular, like the atomic)
        # ---- transport block ----
        if (not percOn) and perc.enabled:
            perc.reset()
        if percOn and (not perc.enabled):
            perc.reset()   # FIX: the counter ran on while held
            if seq.enabled and seq.litStep >= 0:
                wait = seq.samplesToNextStep()
                perc.setStartDelay(wait); seq.restartLegatoIn(wait)
        perc.enabled = percOn
        # ---- seq block, then perc render ----
        seq.process(block, t)
        perc.process(block, t)
        t += block
    # alignment after release: first PERC step 0 after tB vs. nearest figure step 0
    p0 = [ts for ts, s in perc.fired if ts >= tB and s == 0]
    f0 = [ts for ts, s in seq.fired if ts >= tB and s == 0]
    if not p0 or not f0: return ("no restart", block, tA, tB)
    d = min(abs(p0[0] - f) for f in f0)
    # also: every PERC step k after release must coincide with a figure step k
    pairs = {ts: s for ts, s in seq.fired if ts >= p0[0]}
    mism = [(ts, s, pairs.get(ts)) for ts, s in perc.fired if ts >= p0[0] and pairs.get(ts) != s]
    return (d, len(mism), block, tA, tB)

bad = 0
for seed in range(300):
    r = run(seed)
    if r[0] != 0 or r[1] != 0:
        bad += 1
        if bad <= 8: print("MISMATCH", seed, r)
print("runs: 300, mismatches:", bad)
