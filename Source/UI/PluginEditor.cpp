#include "PluginEditor.h"
#include "../DSP/WavetableBank.h"
#include "../Audio/PresetIO.h"
#include "../Audio/Parameters.h"   // Parameters::ID for the Story-1.3 sample rack

// SynthyLookAndFeel now lives in Source/UI/rack/SynthyLookAndFeel.{h,cpp} (AD-7) —
// the rack framework owns the single shared look.

// --- EnvelopeDisplay ---

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    const float left = b.getX(), right = b.getRight();
    const float top = b.getY(), bottom = b.getBottom();
    const float w = b.getWidth(), h = b.getHeight();

    const float a = pA ? pA->load() : 0.0f;
    const float d = pD ? pD->load() : 0.0f;
    const float s = juce::jlimit(0.0f, 1.0f, pS ? pS->load() : 0.0f);
    const float r = pR ? pR->load() : 0.0f;

    // A fixed-width sustain hold; the rest of the width is split between A/D/R
    // proportional to their durations (so the SHAPE always fills the strip).
    const float sustainW = w * 0.22f;
    const float adrW = w - sustainW;
    const float sum = a + d + r;
    const float aw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (a / sum);
    const float dw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (d / sum);
    const float rw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (r / sum);

    const float susY = bottom - s * h;
    const float xPeak = left + aw;
    const float xSusStart = xPeak + dw;
    const float xSusEnd = xSusStart + sustainW;
    const float xEnd = xSusEnd + rw;

    juce::Path curve;
    curve.startNewSubPath(left, bottom);   // note-on at zero
    curve.lineTo(xPeak, top);              // attack → peak
    curve.lineTo(xSusStart, susY);         // decay → sustain level
    curve.lineTo(xSusEnd, susY);           // sustain hold
    curve.lineTo(xEnd, bottom);            // release → zero

    // Soft fill under the curve, then the stroked line on top.
    juce::Path fill = curve;
    fill.lineTo(left, bottom);
    fill.closeSubPath();
    g.setColour(col.withAlpha(0.14f));
    g.fillPath(fill);

    g.setColour(col.withAlpha(0.9f));
    g.strokePath(curve, juce::PathStrokeType(1.6f));

    // Baseline.
    g.setColour(col.withAlpha(0.25f));
    g.drawLine(left, bottom, right, bottom, 1.0f);
}

// --- SynthyEditor ---

SynthyEditor::SynthyEditor(SynthyProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    // Preset Save / Load (shared .synthy JSON)
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(loadBtn);
    saveBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Save preset", PresetIO::presetsFolder(), "*.synthy");
        auto flags = juce::FileBrowserComponent::saveMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::warnAboutOverwriting;
        presetChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (! f.hasFileExtension("synthy")) f = f.withFileExtension("synthy");
            PresetIO::saveToFile(processor.getAPVTS(), f, f.getFileNameWithoutExtension());
            processor.markPresetClean();   // current state now matches the saved file
            setPresetName(f.getFileNameWithoutExtension());
        });
    };
    loadBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Load preset", PresetIO::presetsFolder(), "*.synthy");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        presetChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            PresetIO::loadFromFile(processor.getAPVTS(), f);
            processor.markPresetClean();   // current state now matches the loaded file
            setPresetName(f.getFileNameWithoutExtension());
        });
    };

    addAndMakeVisible(randomBtn);
    randomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6d28d9));
    randomBtn.onClick = [this] { processor.randomize(); setPresetName("Random"); };

    addAndMakeVisible(resetBtn);
    resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff475569));
    resetBtn.onClick = [this] { processor.resetToDefault(); setPresetName("Init"); };

    // Current-preset display
    presetNameLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaab3c0));
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetNameLabel);
    setPresetName(processor.getCurrentPresetName());   // restored from LiveState

    // On-screen keyboard (shares the processor's MidiKeyboardState → plays the
    // active generators with full ADSR per note, transposed relative to C4).
    keyboard = std::make_unique<juce::MidiKeyboardComponent>(
        processor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange(21, 108);  // A0 .. C8 (full 88-key piano)
    keyboard->setKeyWidth(20.0f);          // overridden in resized() to fill the row width
    keyboard->setKeyPressBaseOctave(kbBaseOctave);
    keyboard->setMidiChannelsToDisplay(1);   // only highlight played (ch.1) notes, not the ch.16 drone
    // Allow playing via the computer keyboard (a, w, s, e, d, ... map to notes;
    // z / x shift the octave; the keyboard must have focus — grabbed on launch/click).
    keyboard->setWantsKeyboardFocus(true);
    addAndMakeVisible(*keyboard);
    juce::Component::SafePointer<juce::MidiKeyboardComponent> kbPtr(keyboard.get());
    juce::MessageManager::callAsync([kbPtr]() mutable { if (kbPtr) kbPtr->grabKeyboardFocus(); });

    // The editor itself must NOT grab keyboard focus either (a click on the empty
    // background would otherwise steal it from the keyboard). z / x still reach our
    // keyPressed via event bubbling up from the focused keyboard component.
    setWantsKeyboardFocus(false);

    // Stop every knob/toggle/combo from grabbing keyboard focus when clicked, so
    // the on-screen keyboard keeps focus and the computer keys keep playing notes
    // even WHILE the user is tweaking parameters. (The keyboard keeps its focus;
    // a slider's right-click value box still grabs focus on demand for typing.)
    // Story 1.3: stand up the sample rack BEFORE dropFocus so its controls are also
    // excluded from grabbing keyboard focus.
    buildSampleRack();

    std::function<void(juce::Component&)> dropFocus = [&](juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (child == keyboard.get())
                continue;   // the keyboard MUST keep keyboard focus
            child->setWantsKeyboardFocus(false);
            dropFocus(*child);
        }
    };
    dropFocus(*this);

    // setSize must be LAST so resized() sees all components. Story 1.3: width stays at
    // the original 1520; the HEIGHT is derived from the rack's actual content so the
    // full rack (incl. the Scope + Spectrum L displays at the bottom) always fits
    // without scrolling. The auto-fit below scales the whole editor down on smaller
    // displays. (kBodyTop/kBodyBottom mirror the bands reserved in resized().)
    constexpr int kDesignW   = 1520;
    constexpr int kBodyTop    = 72;   // header row + gap (matches resized())
    constexpr int kBodyBottom = 72;   // keyboard band (matches resized())
    constexpr int kMargin     = 12;   // getLocalBounds().reduced(12)
    const int rackW = kDesignW - 2 * kMargin;
    const int rackH = sampleRack ? sampleRack->preferredHeight(rackW) : 800;
    const int kDesignH = juce::jmax(1015, rackH + kBodyTop + kBodyBottom + 2 * kMargin);
    setSize(kDesignW, kDesignH);

    // --- Auto-fit ---------------------------------------------------------
    // Backup for displays whose usable area is still smaller than the design
    // canvas (e.g. 1366x768 laptops): scale the WHOLE editor down via a
    // transform. The standalone window sizes itself from
    // getLocalArea(editor, ...) which honours the transform, so the window
    // shrinks to match. Proportions stay intact; we never scale above 1.0.
    if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto ua = disp->userBounds;        // excludes the taskbar
        const double chrome = 90.0;              // title bar + a little breathing room
        const double sH = (ua.getHeight() - chrome) / (double) kDesignH;
        const double sW =  ua.getWidth()           / (double) kDesignW;
        const double scale = juce::jlimit(0.5, 1.0, juce::jmin(sH, sW));
        if (scale < 0.999)
            setTransform(juce::AffineTransform::scale((float) scale));
    }

    // Drive the OSC FREQ-knob display (played frequency).
    startTimerHz(30);
}

void SynthyEditor::timerCallback()
{
    double ratio = processor.getCurrentNoteRatio();

    // Live modulation rings: route the current LFO value to whichever knob(s) the
    // LFO targets (0 Frequency, 1 Amplitude, 2 FilterCutoff), but only when the LFO is
    // enabled (lfoOn).
    auto& apvts = processor.getAPVTS();
    float lfo = processor.getLfoDisplayValue();
    bool lfoActive = *apvts.getRawParameterValue("lfoOn") > 0.5f;
    int target = (int) *apvts.getRawParameterValue("lfoTarget");

    // The live feed drives the rack (AD-8): ONE timer, rack fans out to its frames.
    // ModTarget has a +1 offset vs the raw lfoTarget (ModTarget::None = 0; raw 0 = Frequency).
    if (sampleRack)
    {
        const rack::ModTarget activeT = lfoActive ? static_cast<rack::ModTarget>(target + 1)
                                                  : rack::ModTarget::None;
        sampleRack->updateLiveFeed(lfoActive, activeT, lfo, ratio);
    }

    // Keep the header label in sync: it reacts both to the (async-restored)
    // preset name and to live edits flipping the "modified" flag.
    updatePresetLabel();
}

// A loaded-and-untouched preset shows its name; once any parameter changes
// (and until the user saves) it's an unsaved working state → "Current State".
void SynthyEditor::updatePresetLabel()
{
    auto text = processor.isPresetModified()
                  ? juce::String("Current State")
                  : ("Preset: " + processor.getCurrentPresetName());
    if (text != shownLabel)
    {
        shownLabel = text;
        presetNameLabel.setText(text, juce::dontSendNotification);
    }
}

void SynthyEditor::setPresetName(const juce::String& name)
{
    processor.setCurrentPresetName(name);   // keep the processor (LiveState) in sync
    updatePresetLabel();
}

bool SynthyEditor::keyPressed(const juce::KeyPress& key)
{
    // z / x shift the computer-keyboard octave (these keys aren't note keys).
    auto c = key.getTextCharacter();
    if (c == 'z' || c == 'Z' || c == 'x' || c == 'X')
    {
        int dir = (c == 'z' || c == 'Z') ? -1 : 1;
        kbBaseOctave = juce::jlimit(1, 7, kbBaseOctave + dir);
        if (keyboard)
            keyboard->setKeyPressBaseOctave(kbBaseOctave);
        return true;
    }
    // Spacebar re-plucks the Karplus string. Trigger the actual PLUCK button so it
    // visibly presses too (its onClick calls pluckString); fall back to the direct call.
    if (key == juce::KeyPress::spaceKey)
    {
        if (auto* f = sampleRack ? sampleRack->moduleById("stringkarplus") : nullptr)
            f->clickFirstAction();
        else
            processor.pluckString();
        return true;
    }
    return false;
}

void SynthyEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Title: big "J A S S" with the full name as a small subtitle beneath it.
    {
        auto titleArea = g_titleBounds;
        auto subArea = titleArea.removeFromBottom(20);
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff40c0ff));
        g.drawText("J A S S", titleArea, juce::Justification::centred);
        g.setFont(juce::FontOptions(13.0f));
        g.setColour(juce::Colour(0xff8899aa));
        g.drawText("Just Another Simple Synthesizer", subArea, juce::Justification::centred);
    }
}

void SynthyEditor::buildSampleRack()
{
    // TEMP (Story 1.3): a throwaway population to verify the grid engine, zone headers
    // and shared look at the fixed 1920×1200 target. It mirrors the mockup census
    // (≈10×S, 6×M, 4×L) and binds REAL Parameters::ID values so the frames' APVTS
    // attachments resolve. Story 1.5 replaces this with the real module descriptors.
    using namespace rack;
    auto& apvts = processor.getAPVTS();
    // MASTER BUS is the top row (first zone), then the three main zones below it.
    sampleRack = std::make_unique<Rack>(apvts, Rack::kDefaultCols,
        std::vector<Rack::Zone>{ Rack::Zone::MasterBus, Rack::Zone::Generators,
                                 Rack::Zone::Modulation, Rack::Zone::Processing });

    namespace P = Parameters::ID;

    // small builders to keep the descriptor list readable
    auto K = [](juce::String id, juce::String lbl) { return Knob{ std::move(id), std::move(lbl) }; };
    auto C = [](juce::String id, juce::String lbl, juce::StringArray items)
             { return Combo{ std::move(id), std::move(lbl), std::move(items) }; };
    // Story 1.4 (verification wiring; folded into the real descriptors in 1.5/2.2):
    // a knob tagged as an LFO ring target …
    auto Kmod = [](juce::String id, juce::String lbl, ModTarget mt)
             { Knob k{ std::move(id), std::move(lbl) }; k.modTarget = mt; return k; };
    // … and a FREQ knob with the played-frequency display transform (base × ratio).
    auto Kfreq = [](juce::String id, juce::String lbl)
             {
                 Knob k{ std::move(id), std::move(lbl) };
                 k.modTarget    = ModTarget::Frequency;
                 k.toDisplay    = [](double base,  double ratio) { return base  * ratio; };
                 k.fromDisplay  = [](double shown, double ratio) { return shown / ratio; };
                 return k;
             };

    auto add = [&](Rack::Zone zone, SizeClass sc, ModuleType type, juce::String title,
                   juce::String enableParam, std::vector<BodyElement> body)
    {
        ModuleDescriptor d;
        d.sizeClass = sc; d.type = type;
        // Stable slug from the title (e.g. "OSC 1" -> "osc1") — the RackLayout key for
        // show/hide + drag-drop (AD-10). Derived once here so every module gets one.
        d.id = title.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
        d.title = std::move(title);
        d.defaultZone = zone;   // AD-10: zone declared on the descriptor
        d.enableParam = std::move(enableParam); d.body = std::move(body);
        sampleRack->addModule(std::move(d));
    };

    // OSC WAVE items MUST match the oscWave param's choice ORDER — the ComboBoxAttachment
    // maps by index, so a different order mislabels every waveform (same class of bug as the
    // LFO-WAVE fix in Story 2.1). oscWave = { Sine, Sawtooth, Square, Triangle } (Parameters.h).
    const juce::StringArray waves { "Sine", "Sawtooth", "Square", "Triangle" };

    // ---- MASTER BUS (top row; PROTOTYPE: decisions A+B) ----
    // Stereo becomes a normal module whose Enable IS stereoOn (no special-case header
    // chrome); Master is the new XS class (a single knob). Demonstrates "everything is a
    // module" before we formalise FR14 / the XS size class via correct-course.
    add(Rack::Zone::MasterBus, SizeClass::XS, ModuleType::Processor, "STEREO", P::stereoOn,
        { K(P::stereoWidth, "WIDTH"), K(P::stereoTime, "TIME") });
    add(Rack::Zone::MasterBus, SizeClass::XXS, ModuleType::Processor, "MASTER", P::masterOn,
        { K(P::masterVol, "VOL") });

    // ---- GENERATORS ----
    auto addOsc = [&](int i)
    {
        add(Rack::Zone::Generators, SizeClass::S, ModuleType::Generator,
            "OSC " + juce::String(i), P::oscOn(i),
            { C(P::oscWave(i), "WAVE", waves), Kfreq(P::oscFreq(i), "FREQ"),
              Kmod(P::oscAmp(i), "AMP", ModTarget::Amplitude), K(P::oscUniVoices(i), "VOICES"),
              K(P::oscUniDetune(i), "DETUNE") });
    };
    // MIX MODE (XS, half-width) sits BETWEEN OSC 1 and OSC 2 — it couples OSC 1<->2, so it
    // reads as the connector between them. Row-major packing then puts OSC 3 on row 2 and
    // (after Sub+Noise) Karplus on row 3.
    addOsc(1);
    {
        // MIX MODE couples OSC1<->OSC2 (it sits between them). It is only meaningful when
        // BOTH are on, so it shows as active/lit only then and dims otherwise. The derived
        // enable reads the shared osc1On/osc2On params (AD-9) — it holds NO reference to the
        // OSC modules; the atomics are grabbed once (stable for the APVTS lifetime).
        auto* o1 = apvts.getRawParameterValue (P::oscOn (1));
        auto* o2 = apvts.getRawParameterValue (P::oscOn (2));
        ModuleDescriptor mix;
        mix.sizeClass = SizeClass::XXS; mix.type = ModuleType::Generator;
        mix.id = "mixmode"; mix.title = "MIX MODE";
        mix.enableParam = P::mixModeOn;   // real user enable (off => additive, Story 2.4)
        mix.body = { C(P::mixMode, "MODE", { "Additive", "RingMod", "FM" }) };
        // Effective lit = mixModeOn AND (osc1 && osc2): the interactive toggle is the user's
        // enable; the predicate additionally dims when the coupling is meaningless (a UI cue,
        // not an audio gate — the audio additive-fallback keys off mixModeOn only).
        mix.enabledWhen = [o1, o2] { return o1->load() >= 0.5f && o2->load() >= 0.5f; };
        mix.defaultZone = Rack::Zone::Generators;   // AD-10: zone on the descriptor
        sampleRack->addModule(std::move(mix));
    }
    addOsc(2);
    addOsc(3);

    add(Rack::Zone::Generators, SizeClass::S, ModuleType::Generator, "SUB", P::subOn,
        { C(P::subWave, "WAVE", { "Sine", "Square" }), K(P::subLevel, "LEVEL") });
    add(Rack::Zone::Generators, SizeClass::S, ModuleType::Generator, "NOISE", P::noiseOn,
        { C(P::noiseType, "TYPE", { "White", "Pink" }), K(P::noiseAmp, "AMP") });
    add(Rack::Zone::Generators, SizeClass::M, ModuleType::Generator, "STRING - KARPLUS", P::karplusOn,
        { Action{ "PLUCK", [this] { processor.pluckString(); }, {} },
          K(P::karplusFreq, "FREQ"), K(P::karplusAmp, "AMP"),
          K(P::karplusDamping, "DAMP"), K(P::karplusStretch, "STR") });
    add(Rack::Zone::Generators, SizeClass::M, ModuleType::Generator, "WAVETABLE", P::wavetableOn,
        { Combo{ P::wavetableBank, "BANK",
                 std::function<juce::StringArray()>([] { return WavetableBankStore::instance().getNames(); }) },
          FileAction{ "LOAD WAV",
                      [this] (juce::File f)
                      {
                          int idx = WavetableBankStore::instance().loadWav(f);
                          if (idx >= 0)
                              if (auto* pr = processor.getAPVTS().getParameter(P::wavetableBank))
                                  pr->setValueNotifyingHost(pr->convertTo0to1((float) idx));
                      },
                      { juce::String(P::wavetableBank) } },   // refresh the BANK combo after load
          K(P::wavetablePosition, "POS"), K(P::wavetableFreq, "FREQ"), K(P::wavetableAmp, "AMP"),
          K(P::wavetableUniVoices, "VOICES"), K(P::wavetableUniDetune, "DETUNE") });

    // ---- MODULATION ----
    // ADSR: the second unit-row is the REAL EnvelopeDisplay (attack→decay→sustain→release
    // curve), a Display body element (AD-5), owned by sampleOwned so its lifetime is tied
    // to the editor.
    add(Rack::Zone::Modulation, SizeClass::L, ModuleType::Modulator, "ENVELOPE - ADSR", P::adsrOn,
        { K(P::attack, "ATK"), K(P::decay, "DEC"), K(P::sustain, "SUS"), K(P::release, "REL"),
          Display{ sampleOwned.add(new EnvelopeDisplay(apvts, juce::Colour(0xff22d3ee))), 4 } });
    // LFO WAVE must list the lfoWave param's OWN choices in order — the ComboBoxAttachment
    // maps by index, so the shared `waves` array (a different order) would mislabel every
    // waveform (Story 2.1 AC3).
    add(Rack::Zone::Modulation, SizeClass::M, ModuleType::Modulator, "LFO", P::lfoOn,
        { C(P::lfoWave, "WAVE", { "Sine", "Triangle", "Square", "Sawtooth" }),
          C(P::lfoTarget, "TARGET", { "Frequency", "Amplitude", "Filter Cutoff" }),
          K(P::lfoRate, "RATE"), K(P::lfoDepth, "DEPTH") });
    add(Rack::Zone::Modulation, SizeClass::M, ModuleType::Modulator, "ARPEGGIATOR", P::arpOn,
        { C(P::arpMode, "MODE", { "Up", "Down", "UpDown", "Random" }),
          K(P::arpRate, "RATE"), K(P::arpOctaves, "OCT"), K(P::arpGate, "GATE") });

    // ---- PROCESSING ----
    // FILTER: TYPE combo + CUTOFF + RESO (= 4 slots, like DISTORTION) → M (4 cols) so the
    // combo isn't cramped. (Exact width tuning deferred to next session.)
    add(Rack::Zone::Processing, SizeClass::M, ModuleType::Processor, "FILTER", P::filterOn,
        { C(P::filterType, "TYPE", { "Lowpass", "Highpass" }),
          Kmod(P::filterCutoff, "CUTOFF", ModTarget::FilterCutoff), K(P::filterReso, "RESO") });
    // M-class so the TYPE combo (2 slots) fits alongside DRIVE + MIX.
    // DISTORTION TYPE: display text is cosmetic ("Soft Clip"/"Hard Clip" read better) — the
    // ComboBoxAttachment maps by INDEX, so the canonical param/.synthy strings stay
    // "SoftClip"/"HardClip" (project-context: UI display may differ from the interop string).
    // Order/count MUST match distortionType's choices exactly, or the index mapping breaks.
    add(Rack::Zone::Processing, SizeClass::M, ModuleType::Processor, "DISTORTION", P::distortionOn,
        { C(P::distortionType, "TYPE", { "Soft Clip", "Hard Clip", "Foldback" }),
          K(P::distortionDrive, "DRIVE"), K(P::distortionMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "WAVEFOLD", P::wavefoldOn,
        { K(P::wavefoldDrive, "DRIVE"), K(P::wavefoldSymmetry, "SYM"), K(P::wavefoldMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "BITCRUSH", P::bitcrushOn,
        { K(P::bitcrushBits, "BITS"), K(P::bitcrushRate, "RATE"), K(P::bitcrushMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "CHORUS", P::chorusOn,
        { K(P::chorusRate, "RATE"), K(P::chorusDepth, "DEPTH"), K(P::chorusMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "DELAY", P::delayOn,
        { K(P::delayTime, "TIME"), K(P::delayFeedback, "FB"), K(P::delayMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "REVERB", P::reverbOn,
        { K(P::reverbRoom, "ROOM"), K(P::reverbDamp, "DAMP"), K(P::reverbMix, "MIX") });
    // Real visualizers (own instances, separate from the legacy ones behind the rack —
    // one-parent rule). setShowTitle(false): the module header already shows the title.
    // Sharing the one WaveformCapture across instances is safe (updateSnapshot is idempotent
    // per frame). Sample rate reaches them via the capture (set in prepareToPlay).
    auto* scope = new WaveformDisplay(processor.getWaveformCapture());
    scope->setShowTitle(false);
    scope->setEnableSource(apvts.getRawParameterValue(P::scopeOn));   // scopeOn off => freeze+blank
    sampleOwned.add(scope);
    add(Rack::Zone::Processing, SizeClass::XL, ModuleType::Processor, "OSCILLOSCOPE", P::scopeOn,
        { Display{ scope, 12 } });
    auto* spec = new SpectrumDisplay(processor.getWaveformCapture());
    spec->setShowTitle(false);
    spec->setEnableSource(apvts.getRawParameterValue(P::spectrumOn));   // spectrumOn off => freeze+blank
    sampleOwned.add(spec);
    add(Rack::Zone::Processing, SizeClass::XL, ModuleType::Processor, "SPECTRUM", P::spectrumOn,
        { Display{ spec, 12 } });

    // Added LAST so the opaque rack covers the legacy body; the header chrome and
    // keyboard sit in their own bands and stay live.
    addAndMakeVisible(*sampleRack);
}

void SynthyEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // ===== Header — Save/Load/Random/Reset left, Title + Current State center =====
    // (Master + Stereo moved out of the header into the MASTER BUS rack zone.) The header
    // is kept compact; the freed space gives the title + preset name room to breathe.
    auto headerRow = area.removeFromTop(64);
    // The title is centred over the FULL header width so "J A S S" sits in the true middle
    // of the window; the left cluster only overlays the left edge, clear of the centred text.
    g_titleBounds = headerRow;
    // Left cluster: the Save/Load/Random/Reset buttons AND the current-preset name belong
    // together (the preset name is about what was loaded/saved). Buttons in a 2x2 block
    // with "Current State" beside them.
    auto leftGroup = headerRow.removeFromLeft(340);
    auto leftBtns = leftGroup.removeFromLeft(150);
    auto row1 = leftBtns.removeFromTop(30);
    saveBtn.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(3, 2));
    loadBtn.setBounds(row1.reduced(3, 2));
    auto row2 = leftBtns.removeFromTop(30);
    randomBtn.setBounds(row2.removeFromLeft(row2.getWidth() / 2).reduced(3, 2));
    resetBtn.setBounds(row2.reduced(3, 2));
    leftGroup.removeFromLeft(10);
    presetNameLabel.setBounds(leftGroup);   // grouped with the load/save controls
    area.removeFromTop(8);

    // The body between the header and keyboard belongs entirely to the rack (below).
    // Reserve the full-width keyboard band at the bottom; the rack fills the rest.
    auto kbRow = area.removeFromBottom(72).reduced(3, 0);

    if (keyboard)
    {
        keyboard->setBounds(kbRow.reduced(2));
        // Spread the configured range across the full row width instead of
        // leaving blank space to the right: size each key so all white keys
        // in the range exactly fill the keyboard.
        int whiteKeys = 0;
        for (int n = keyboard->getRangeStart(); n <= keyboard->getRangeEnd(); ++n)
            if (! juce::MidiMessage::isMidiNoteBlack(n)) ++whiteKeys;
        if (whiteKeys > 0)
            keyboard->setKeyWidth((float) keyboard->getWidth() / (float) whiteKeys);
    }

    // The rack fills the body band (below the header row, above the keyboard); the header
    // chrome and keyboard keep their own bands.
    if (sampleRack)
    {
        auto rb = getLocalBounds().reduced(12);
        rb.removeFromTop(64 + 8);    // header row + gap (mirrors the header band above)
        rb.removeFromBottom(72);     // keyboard band
        sampleRack->setBounds(rb);
    }
}
