# Synthesizer Project

Research and evaluation phase for building a custom software synthesizer.

## Technology Stack Decision

### Recommended Language: C++ with JUCE Framework

**JUCE** (Jules' Utility Class Extensions) is the de-facto standard framework for audio plugin development.

- Audio DSP engine (oscillators, filters, effects, MIDI)
- Plugin export: VST3, AU, AAX, CLAP, Standalone — from a single codebase
- Built-in GUI framework with knobs, sliders, spectrum analyzers
- Cross-platform: Windows, macOS, Linux, iOS, Android
- Project management via **Projucer**
- License: GPLv3 (free/open source) or commercial

### Alternative Stacks

| Language | Libraries | Notes |
|---|---|---|
| **Rust** | cpal, fundsp, nih-plug | Modern, memory-safe, growing audio community |
| **C#** | NAudio, SoundFlow | Possible but limited ecosystem for DSP |
| **JavaScript** | Tone.js, Web Audio API | Good for prototyping, browser-based |
| **Faust** | (standalone DSP language) | Compiles to C++, VST, Web Audio |

## Open Source Synthesizers — Evaluation Candidates

All synths below are to be installed and tested before choosing a base project.

### Surge XT
- **Type:** Hybrid (subtractive, wavetable, FM, additive)
- **Stack:** C++ / JUCE | GPLv3
- **Website:** https://surge-synthesizer.github.io
- **Pros:** Very active community, excellent code documentation, versatile engine, developer wiki + Discord, accessibility features
- **Cons:** Large codebase, historically grown (some inconsistencies), functional but not flashy UI
- **Best for:** Building on an existing, actively maintained project

### Vital
- **Type:** Wavetable synth
- **Stack:** C++ / JUCE | GPLv3
- **Website:** https://vital.audio
- **Pros:** Professional quality, stunning UI with real-time visualizations, wavetable editor, visual modulation matrix
- **Cons:** Very complex codebase (~150k lines), main developer less active, fragmented forks, GPU-dependent UI
- **Best for:** Learning wavetable synthesis at a professional level

### Odin 2
- **Type:** Semi-modular synth
- **Stack:** C++ / JUCE | GPLv3
- **Website:** https://thewavewarden.com/odin2
- **Pros:** Semi-modular design with flexible routing, multiple oscillator types, modern UI, balanced complexity
- **Cons:** Smaller community, fewer presets, limited documentation
- **Best for:** Understanding semi-modular routing architectures

### Helm
- **Type:** Polyphonic subtractive synth
- **Stack:** C++ / JUCE | GPLv3
- **Website:** https://tytel.org/helm
- **Pros:** Clean architecture, best entry point for learning, well-structured code
- **Cons:** No longer actively developed (succeeded by Vital), older JUCE version
- **Best for:** Learning synth development fundamentals

### Dexed
- **Type:** FM synth (Yamaha DX7 emulation)
- **Stack:** C++ / JUCE | GPLv3
- **Website:** https://asb2m10.github.io/dexed
- **Pros:** Compact and readable code, faithful DX7 emulation, thousands of SysEx patches available
- **Cons:** FM synthesis only, limited extensibility, dated UI
- **Best for:** Learning FM synthesis

### OB-Xd
- **Type:** Subtractive synth (Oberheim OB-X emulation)
- **Stack:** C++ / JUCE | GPLv3
- **Website:** https://github.com/reales/OB-Xd/releases
- **Pros:** Classic warm analog sound, small and understandable codebase
- **Cons:** Less active development, limited modulation, no wavetable/FM
- **Best for:** Understanding analog emulation / virtual analog synthesis

### ZynAddSubFX (Zyn-Fusion)
- **Type:** Additive, subtractive, pad synthesis
- **Stack:** C++ (custom framework, not JUCE) | GPLv2
- **Website:** https://zynaddsubfx.sourceforge.io
- **Pros:** Unique additive synthesis engine, extremely deep sound design, mature project
- **Cons:** Custom UI framework (harder to extend), historically grown code, steep learning curve
- **Best for:** Exploring additive synthesis

### VCV Rack
- **Type:** Modular Eurorack simulator
- **Stack:** C++ (custom SDK, not JUCE) | GPLv3
- **Website:** https://vcvrack.com
- **Pros:** Fully modular system, huge plugin ecosystem (1000+ modules), well-documented module development
- **Cons:** No VST export in open-source version, custom SDK, more of an ecosystem than a single synth
- **Best for:** Building individual modules within a modular environment

## Comparison Matrix

| Project | Learning | Extensibility | Community | Code Quality |
|---|---|---|---|---|
| **Surge XT** | medium | very good | very active | very good |
| **Vital** | hard | medium | medium | good |
| **Odin 2** | good | good | small | good |
| **Helm** | very good | medium | inactive | very good |
| **Dexed** | very good | limited | small | good |
| **OB-Xd** | good | medium | small | medium |
| **VCV Rack** | medium | very good | very active | good |
| **ZynAddSubFX** | hard | medium | medium | medium |

## DAW for Testing

**Chosen: REAPER** (https://www.reaper.fm)

- Unlimited trial with no restrictions
- ~30 MB install, starts instantly
- Reliable VST3 plugin support
- Lightweight and ideal for plugin testing

### Other Free DAW Options Considered

| DAW | Notes |
|---|---|
| **Cakewalk by BandLab** | Full-featured, free, Windows only |
| **Ardour** | Open source (GPLv2), more recording-focused |
| **Waveform Free** | Modern UI, good VST3 support |
| **LMMS** | Open source, electronic music focus, limited VST support |
| **FL Studio Trial** | Full features but cannot reopen saved projects |

## Next Steps

1. Install REAPER
2. Download and install all candidate synths as VST3 plugins
3. Test each synth for sound, workflow, and UI
4. Choose a base project
5. Set up the development environment (C++ / JUCE)
6. Fork and start customizing
