# Vendored: Signalsmith Stretch (Story 12.3)

Polyphonic pitch-shift / time-stretch library by Geraint Luff / Signalsmith Audio Ltd.,
used by the SAMPLER's STRETCH mode (pitch/time decoupling). **MIT license** — see
`LICENSE.txt` (stretch) and `signalsmith-linear/LICENSE.txt` (its FFT/STFT dependency).
Attribution also lives in the top-level README ("Third-party").

- `signalsmith-stretch.h` — github.com/Signalsmith-Audio/signalsmith-stretch @ `57b93f4`
- `signalsmith-linear/{stft,fft}.h` — github.com/Signalsmith-Audio/linear @ `0dd6b82`

Deliberately vendored as a plain copy (like the KEMAR data, unlike the JUCE submodule):
three headers, no build glue. The optional SIMD backends (`SIGNALSMITH_USE_*` defines +
`platform/` headers upstream) are NOT vendored — we compile the portable plain-C++ path,
which is also what the Story 12.3 bake-off measured. To update: copy the same three files
from upstream, keep the license files, update the SHAs above, and re-run the bake-off
(`scratchpad/bakeoff` harness in the 12.3 story's Dev Agent Record).
