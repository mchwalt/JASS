#pragma once
#include <JuceHeader.h>
#include <bitset>
#include "Audio/SynthVoice.h"
#include "Audio/SynthSound.h"
#include "Audio/Parameters.h"
#include "Audio/PatchEngine.h"   // Story 21.4: the patch's engine (AD-16: one per layer)
#include "Audio/LayerRouter.h"   // Story 21.5: LAYER / SPLIT in front of the two engines
#include "DSP/WaveformCapture.h"
#include "DSP/StereoWidth.h"
#include "DSP/BinauralRoom.h"
#include "DSP/Compressor.h"
#include "DSP/Arpeggiator.h"
#include "DSP/StepSequencer.h"   // Story 15.1
#include "Audio/PatternLane.h"    // Story 21.8: one STEP SEQ + ARP lane per layer
#include "DSP/PercSequencer.h"   // Story 16.1 — layer B: four percussion tracks on the master bus
#include "Audio/PatternStore.h"  // AD-14 (18.5): the sequencer patterns — content, not parameters
#include "Audio/ControllerProfiles.h"   // Story 22.1: hardware knobs / faders -> parameters

// DIAGNOSTIC BUILD SWITCH (2026-10-20, not for release): log the first blocks after each host PLAY
// to %AppData%\JASS\hostsync.log — host position, play flag, the STEP SEQ clock before and after
// syncToHost, and the step that fired. Set to 0 (or delete the blocks) once the start-up bug is found.
#ifndef JASS_HOSTSYNC_DIAG
 #define JASS_HOSTSYNC_DIAG 0
#endif
#include "DSP/ChaosLorenz.h"     // LFO expansion — global Lorenz mod source (Chaos X/Y)
#include <vector>
#include <map>
#include <unordered_map>

class SynthyProcessor : public juce::AudioProcessor,
                        private juce::Timer,
                        private juce::ValueTree::Listener,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::MidiKeyboardState::Listener
{
public:
    SynthyProcessor();
    ~SynthyProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "JASS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    // Infinite: JASS is never idle in a host. STEP SEQ and PERC play from the transport alone, with
    // no note coming in, and a host that "trusts the plug-in" (Bitwig's default Suspend mode)
    // stops calling a plug-in that reports no tail once its input and output are silent - the
    // PERC track then stayed mute until its window was opened (maintainer 2026-10-25, Bitwig:
    // "spielt erst, wenn ich die Spur anklicke"; the plug-in logged no blocks while the host
    // played). An infinite tail (VST3 kInfiniteTail) keeps the process call coming.
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    WaveformCapture& getWaveformCapture() { return waveformCapture; }

    // Channel the editor previews a STEP SEQ step on (Story 15.3). It cannot be channel 1: while
    // the sequencer (or the ARP) runs, every channel-1 note in the buffer is dropped so that only
    // the pattern sounds — which is exactly when the preview is wanted. 15 passes through
    // untouched, the same trick the auto-play drone plays on 16.
    static constexpr int kAuditionChannel = 15;
    // ...and layer B's STEP SEQ previews on 12 (Story 21.8): the router sends 15 to the edited layer
    // and 12 always to B, so a step of B's figure is heard through B whatever the selection.
    static constexpr int kAuditionChannelB = 12;
    static constexpr int auditionChannelOf (int lane) { return lane == 0 ? kAuditionChannel : kAuditionChannelB; }
    // The auto-play drone's own channel (see autoPlayEnabled). Public because the editor has to
    // recognise it: a drone note is not a played key, so it must not be written into a STEP SEQ
    // step while a figure is being recorded (Story 15.4).
    static constexpr int kDroneChannel = 16;
    // The pattern channels (AD-16 S3, Story 21.7; one per lane since 21.8): layer A's STEP SEQ / ARP
    // play channel 14, layer B's channel 13, and the router sends each to its own layer, whatever
    // the pitch, while channel 1 stays the live keys. Note-ons sound alike on every channel
    // (SynthSound applies to all), but note-offs are matched BY channel: mono glide releases on
    // the sounding note's own channel, and a host panic on any other channel releases these too
    // (review 2026-11-18).
    static constexpr int kPatternChannel  = 14;   // lane 0 = layer A
    static constexpr int kPatternChannelB = 13;   // lane 1 = layer B
    static constexpr int kNumLanes = PatternStore::kStepLanes;
    static constexpr int patternChannelOf (int lane) { return lane == 0 ? kPatternChannel : kPatternChannelB; }

    // While the editor is recording a figure into STEP SEQ (Story 15.4), a played key WRITES a step
    // instead of playing the pattern: the sequencer must not take it as its root, or every entered
    // note would restart and transpose the figure under the writer's hands. Set from the message
    // thread, read once per block — the editor clears it when it stops recording and in its
    // destructor, so a closed window can never leave the sequencer rootless.
    void setSeqRecordArmed (int lane, bool armed) { lanes[(size_t) lane].recordArmed.store (armed); }

    // The whole state was replaced behind an editor that already exists — it was built against
    // a state that is now gone and must apply the stored rack layout again. Two callers:
    // (1) standalone: the shared LiveState is re-loaded ONCE asynchronously after construction,
    // because the wrapper restores its own saved state in between (see the constructor);
    // `fromScratch` = no LiveState existed and Init was run instead, the rack then starts from
    // the player's default layout. (2) host: setStateInformation — a DAW that reopens plugin
    // windows BEFORE it hands the project state over (Bitwig does) had the editor show the
    // stock rack, STEP SEQ and PERC hidden, while both played (maintainer 2026-10-19).
    // Message thread; the editor clears it on close.
    std::function<void (bool fromScratch)> onStateReloaded;

    // Re-pluck the Karplus string on every voice (PLUCK button / spacebar). The flag
    // is consumed on the audio thread in processBlock — RT-safe (no direct voice touch
    // from the message thread).
    void pluckString()
    {
        pluckRequested = true;
        // A pluck only re-excites ACTIVE voices; after the keyboard was played the auto-play
        // drone is off and no voice renders, so the pluck would be silent. If nothing is held,
        // re-arm the drone so a voice is active and the pluck is actually heard.
        if (heldNotesLo.load() == 0 && heldNotesHi.load() == 0)
            autoPlayEnabled = true;
    }

    // Randomize all parameters (with guards so the result stays audible).
    void randomize();

    // Reset everything to defaults, then apply the Init patch (initPatchOverrides, both layers).
    void resetToDefault();
    // The Init patch where it differs from the parameters' declared defaults (plain values), for
    // both layers: A = OSC 1..3 on, MATRIX off; B (21.6) = a punchy saw bass (filter swept by LFO 1
    // One-Shot), so a Split sounds two clearly different sounds at once. The declared defaults stay untouched (old presets
    // read missing => default). RESET applies it, Shift+reset and zone resets go back to it.
    static const std::vector<std::pair<juce::String, float>>& initPatchOverrides();

    // Pitch ratio of the note currently being played (relative to C4 = note 60);
    // 1.0 when only the drone or nothing sounds. Used by the editor so the OSC
    // FREQ knobs can display the actually-played frequency.
    float getCurrentNoteRatio() const { return currentNoteRatio.load(); }

    // Which step PERC is on, for the grid's playhead (Story 16.1). Plain atomic read.
    int getPercStep() const { return percStepDisplay.load(); }
    // The sequencer patterns (AD-14): the grid writes cells here from the message thread, the
    // audio thread reads them per block. Not parameters — a DAW never sees them as such.
    PatternStore&       getPatterns()       noexcept { return patterns; }
    const PatternStore& getPatterns() const noexcept { return patterns; }
    // Per lane (Story 21.8; lane = layer, 0 = A, 1 = B):
    // MIDI note the lane's STEP SEQ is sounding, or -1. For the on-screen keyboard only.
    int getSeqNote (int lane) const { return lanes[(size_t) lane].noteDisplay.load(); }
    // Step the lane's STEP SEQ is on (0-based), or -1. Drives the module's playhead, like PERC's grid.
    int getSeqStep (int lane) const { return lanes[(size_t) lane].stepDisplay.load(); }
    // One step's length in seconds as last resolved (SYNC against the tempo, or RATE). The GATE
    // preview plays a step for gate% of this, so what you hear while turning is what the figure
    // will play (maintainer 2026-10-10: the preview held every note the same length).
    double getSeqStepSeconds (int lane) const { return lanes[(size_t) lane].stepSecondsDisplay.load(); }

    // Move every LATCHED sequencer root. The pattern keeps running after the key is released, so
    // an octave shift has no held note left to move — the editor sends the ±12 here instead, and
    // each figure follows the octave keys exactly as a held key would have moved it.
    // Only the layer being edited moves (maintainer 2026-11-24, review E1): A, B, or both with A+B
    // — in a Split the other hand's figure stays where it is.
    void transposeSeqLatch (int semitones)
    {
        const int focus = getLayerFocus();
        for (auto& l : lanes)
            if (focus == 2 || focus == l.index)
                if (const int r = l.latchedRoot.load(); r >= 0)
                    l.latchedRoot.store (juce::jlimit (0, 127, r + semitones));
    }
    // Is any latched figure running? (SPACE stops them — the editor needs to know whether there is
    // anything to stop before it falls through to the Karplus pluck.)
    bool isSeqLatched() const
    {
        for (const auto& l : lanes)
            if (l.latchedRoot.load() >= 0) return true;
        return false;
    }
    bool isSeqLatched (int lane) const { return lanes[(size_t) lane].latchedRoot.load() >= 0; }

    // True when a DAW hosts us (VST3). The host then owns mute, level and tempo, and the MASTER
    // module is shown locked + dimmed in the rack (AD-13, Story 18.1): masterOn/masterVol are
    // ignored, the tempo is the host's, and the MasterVol/MasterTempo mod targets are dead.
    bool isHostedByDaw() const noexcept { return wrapperType != wrapperType_Standalone; }
    int  getSeqLatchRoot (int lane) const { return lanes[(size_t) lane].latchedRoot.load(); }   // -1 when not latched
    // Stop every latched figure. Clearing the root is enough: the next block sees nothing playing,
    // releases the sounding note and re-arms the pattern at step 0, exactly as letting go of the
    // key did before the latch existed.
    void stopSeqLatch() { for (auto& l : lanes) { l.latchedRoot.store (-1); l.anchorPpq.store (-1.0); } }
    // Start a lane's latched figure without a key — used when a preset whose STEP SEQ is ON is
    // loaded, so the patch plays itself the moment it arrives instead of waiting to be touched. It
    // also ends the auto-play drone: the instrument IS playing now, and hearing a held C4 under a
    // bass figure is exactly the confusion this is meant to remove.
    void startSeqLatch (int lane, int midiNote)
    {
        lanes[(size_t) lane].latchedRoot.store (juce::jlimit (0, 127, midiNote));
        autoPlayEnabled.store (false);
    }

    // Sound one PERC lane once — the grid plays a step as it is placed. Message thread → audio
    // thread through one atomic; the block consumes it.
    void auditionPercLane (int lane) { percAuditionLane.store (lane); }
    // STOP latch in the PERC header (maintainer 2026-10-10: a way to pause the drums while
    // editing the grid). Held = the pattern stands still and the playhead goes out; the module
    // stays on, the kit stays loaded, and a grid click still sounds its lane. Releasing it
    // starts the pattern on step 1 (the falling-edge reset in processBlock does that). Runtime
    // state like COPY, not a parameter: it is a hand on the transport, not part of the patch.
    void holdPerc (bool hold) { percHeld.store (hold); }
    bool isPercHeld() const   { return percHeld.load(); }

    // Current LFO oscillation value (-1..+1, already scaled by depth) for the
    // editor's live modulation rings. Driven by a dedicated display LFO that
    // mirrors the patch's LFO params and runs even when no note sounds.
    // Per-LFO (for the rings) — of the layer the rack edits (AD-16: A unless the focus is B).
    float getLfoDisplayValue(int i) const
    {
        return (isFocusB() ? engineB : engineA).getLfoDisplayValue(i);
    }
    // ...of a given layer: the A+B view shows frames of both layers at once (2026-11-23).
    float getLfoDisplayValueOf(bool layerB, int i) const
    {
        return (layerB ? engineB : engineA).getLfoDisplayValue(i);
    }
    // 20.1: a preset / DAW-state load restarts every GLOBAL LFO (a global One-Shot then sweeps once
    // from "now"). Message thread sets, the audio thread consumes it once. Both layers.
    void requestGlobalLfoRearm() noexcept { engineA.requestGlobalLfoRearm(); engineB.requestGlobalLfoRearm(); }
    // Voices whose key is down in layer A / B (JASS_Render --selftest: hanging notes, mono glide).
    int layerKeysDown (bool layerB) const { return (layerB ? engineB : engineA).keysDown(); }
    float getChaosDisplayValue(int i) const { return chaosDisplay[i & 1].load(); } // 0=X, 1=Y (for the rings)
    float getModWheelValue() const { return modWheel.load(); }                     // 22.2: CC 1, 0..1 (rings, selftest)

    // Name of the currently loaded/active preset (shown in the header). It is
    // persisted into the LiveState file so it survives a restart. Touched only
    // on the message thread (editor + LiveState save).
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

    // "Modified since last load/save?" → the header shows "Current State" instead
    // of the (now stale) preset name. Determined by comparing the live parameter
    // values against a snapshot taken at the last clean point (load/save/random/
    // reset). Done by value-compare rather than a change-listener because APVTS
    // delivers value-tree change callbacks asynchronously, which would race the
    // "clear after load" and leave the flag permanently set.
    bool isPresetModified() const;
    void markPresetClean();                       // snapshot current values as the clean baseline
    void markLayerBClean();                       // ...for layer B only, after a load into B (keeps A's)
    void restoreModifiedState(bool modified);     // on LiveState load: clean baseline, or force "modified"

    // Story 22.1 — controller profiles (standalone only; in a DAW the host maps controllers itself).
    // Message thread. The header combo lists the names; "" = Auto (picked from the enabled MIDI
    // inputs), anything else is the hand pick, remembered in Settings.json.
    juce::StringArray getControllerProfileNames() const;
    juce::String getControllerProfileChoice() const { return controllerHandPick; }
    juce::String getActiveControllerProfile() const;
    void setControllerProfileChoice (const juce::String& name);
    int  getControllerProfilesRevision() const noexcept { return controllerRevision; }   // bumps on a reload
    // JASS_Render --selftest only: make `profiles[0]` the active profile, then run the CC path
    // (captureControllers + pollControllers) on `midi` — whatever the wrapper type.
    void runControllersForTest (std::vector<ControllerProfiles::Profile> profiles, const juce::MidiBuffer& midi);
    bool anyControllerCapturedForTest() const noexcept
    {
        for (const auto& d : ccDirty) if (d.load() != 0) return true;
        return false;
    }

    // Story 22.5 — knobs follow the module. Message thread. The processor owns the selection and
    // its bank; the editor pushes what the rack shows (only on change) and reads the selection back
    // to draw the marker. A frame is keyed by the rack's layout-entry id (the layer-A id) plus the
    // layer of the frame on screen — never by an index.
    struct ControllerFrame
    {
        juce::String entryId;                                    // layout-entry id (layer-A id)
        bool layerB = false;                                     // the frame on screen is B's
        std::vector<ControllerProfiles::ModuleControl> controls; // countable controls, all banks, display order
        juce::String enableParam;                                // the frame's on/off parameter as it uses it (B frame: the "b" id); "" = none
    };
    struct ControllerSelection
    {
        bool valid = false;
        juce::String entryId;
        bool layerB = false;
        int bank = 0, bankCount = 0;                             // bank is 0-based; the marker shows bank + 1 "/" bankCount
    };
    // The selectable frames on screen in reading order (frames without a countable control are
    // dropped). Keeps the selection while its module is still shown — on the other layer's frame
    // too (a swap: flag flips, bank kept and clamped) — and drops it when the module is gone.
    void setControllerFrames (std::vector<ControllerFrame> frames);
    // A click / touch. False = refused: the active profile has no "@module" / "@select" entry, or
    // the frame is not in the list. Selecting the selected module again keeps its bank.
    bool selectControllerModule (const juce::String& entryId, bool layerB);
    void clearControllerSelection();                             // the editor calls it on a preset load: nothing selected afterwards
    ControllerSelection getControllerSelection() const;
    int  getControllerSelectionRevision() const noexcept { return controllerSelRevision; }   // bumps on every change
    bool controllerProfileSelectsModules() const;                // the active profile has "@module" / "@select"
    // JASS_Render --selftest only.
    bool controllerShiftHeldForTest() const noexcept { return ccShiftHeld.load(); }
    void controllerInputsChangedForTest() { controllerInputsChanged(); }

private:
    // Story 12.6: the startup preload of the Samples folder runs here instead of blocking the
    // constructor. Four-layer piano sets are ~1.2 GB decoded, which used to be dead time before
    // the window even appeared. Sets appear in the SET combo as they arrive; anything a preset
    // actually needs is still resolved by name on demand (PresetIO), so sound is never missed.
    struct SamplePreloadThread : juce::Thread
    {
        SamplePreloadThread (SynthyProcessor& p) : juce::Thread ("JASS sample preload"), owner (p) {}
        void run() override;

        // "A preset wants this set." Jumps the queue: the loader finishes its current set, then
        // loads this one and selects it, before carrying on with the rest of the folder. Called
        // from the message thread (preset load), read by the loader — hence the lock.
        // `targetParamId` is the selector the set belongs in once it arrives (the SAMPLER's SET or
        // PERC's KIT) — a patch can be waiting for one of each, so the request carries its own
        // destination instead of the loader assuming the sampler.
        void request (const juce::String& setName, const juce::String& targetParamId);
        // Generation of the newest request for THAT selector. Per-selector, not global: a patch
        // asking for an instrument and a kit issues two requests, and a single counter would let
        // the second declare the first one stale.
        int  currentGeneration (const juce::String& targetParamId) const;
        // Withdraw the pending request for that selector (queued or already loading).
        void cancel (const juce::String& targetParamId);

    private:
        struct Request { juce::String name, target; int gen = 0; };
        Request takeRequest();
        void loadRequested (const Request& r);
        SynthyProcessor& owner;
        juce::CriticalSection requestLock;
        std::vector<Request> queue;             // at most one entry per selector
        // Generation per selector (samplerSet, bsamplerSet, percKit — AD-16 added the third).
        std::map<juce::String, int> generations;   // guarded by requestLock
    };
    SamplePreloadThread samplePreload { *this };

    // Select a sample set by index (message thread). Used by the loader once a requested set
    // has arrived; `generation` guards against a preset that was loaded in the meantime.
    void selectSamplerSet (int index, int generation, const juce::String& targetParamId);

    juce::AudioProcessorValueTreeState apvts;
    // The two patches (AD-16; Story 21.4 made the engine, 21.5 the second one): voices, glide,
    // GLOBAL/display LFOs, the SAMPLER clock and the per-voice parameter push. Layer A = today's ids
    // (no prefix), layer B = the same ids prefixed "b". Declared right after apvts (they resolve their
    // parameter pointers in their constructors) and A FIRST: every LFO draws its random seed from a
    // process-wide counter, so A must keep the seeds it had before layer B existed.
    PatchEngine engineA { apvts, {} };
    PatchEngine engineB { apvts, Modules::kLayerBPrefix };
    LayerRouter layerRouter;                       // 21.5: LAYER / SPLIT, note-offs follow their note-on
    juce::MidiBuffer layerMidiA, layerMidiB;       // the router's two outputs (pre-sized, audio thread)
    juce::AudioBuffer<float> layerBRender;         // B renders here, then joins the mix (pre-sized)
    float layerGainPrev[2][2] { { 1.0f, 1.0f }, { 1.0f, 1.0f } };   // [layer][L/R] last block's level×balance
    PatchEngine& focusedEngine() noexcept;         // the layer the rack edits (layerFocus)
public:
    // layerFocus: 0 = A, 1 = B, 2 = A+B (linked editing — the rack shows A, edits mirror to B).
    int  getLayerFocus() const { return juce::roundToInt (apvts.getRawParameterValue(Parameters::ID::layerFocus)->load()); }
    bool isFocusB() const      { return getLayerFocus() == 1; }
    // Which layers take live notes (21.6): the selection (A, B, or both), and in a Split both.
    struct LayersPlaying { bool a = true, b = false; };
    // Which keys reach which layer this block (Story 21.8): a lane takes the keys that reach its
    // layer — the selection outside a Split, its key range inside one.
    PatternLane::KeyScope keyScope (const LayersPlaying& playing) const
    {
        PatternLane::KeyScope k;
        k.playA = playing.a;
        k.playB = playing.b;
        k.split = juce::roundToInt (apvts.getRawParameterValue(Parameters::ID::layerMode)->load()) == 2;
        k.splitNote = juce::roundToInt (apvts.getRawParameterValue(Parameters::ID::splitNote)->load());
        return k;
    }
    LayersPlaying layersPlaying() const
    {
        const int mode = juce::roundToInt (apvts.getRawParameterValue(Parameters::ID::layerMode)->load());
        // Split plays both; otherwise the selection decides (A+B = both). The MODE knob is gone
        // (maintainer 2026-11-23): a stored "Dual" (1) from before plays like the selection.
        if (mode == 2) return { true, true };
        const int f = getLayerFocus();
        return { f != 1, f != 0 };
    }
    // LAYERS · LEARN (21.6): arm, then the next played key is reported once (-1 = none yet).
    void armSplitLearn (bool armed)  { learnedSplitNote.store (-1); splitLearnArmed.store (armed); }
    bool isSplitLearnArmed() const   { return splitLearnArmed.load(); }
    int  takeLearnedSplitNote()      { return learnedSplitNote.exchange (-1); }
    std::atomic<bool> splitLearnArmed { false };
    std::atomic<int>  learnedSplitNote { -1 };
private:
    void applyLayerGain (int layer, juce::AudioBuffer<float>& b, int numSamples);
    juce::MidiKeyboardState keyboardState;
    // Snapshot length = the LONGEST scope window (100 ms) at the highest sample rate we
    // expect (96 kHz): 0.100 s × 96000 = 9600. The scope shows only the first N samples of
    // this for the selected window, so a too-small buffer would clamp the long windows to
    // identical traces. Sized once here (never reallocated) so the GUI's updateSnapshot never
    // races a resize. (Spectrum reads at most fftSize=1024 of it, zero-padding when shorter.)
    WaveformCapture waveformCapture { 9600 };
    StereoWidth stereoWidth;   // final pseudo-stereo stage (mono engine -> stereo)
    BinauralRoom binauralRoom; // Kunstkopf externalization: shared early-reflection stage (Story 10.4)
    bool wasKunstkopf = false; // mode-transition edge -> reset the reflection ring (no stale audio)
    Compressor  compressor;    // master-bus compressor (runs on the summed mix, before width)
    // CHAOS: ONE global Lorenz attractor (all voices move together — correlated, musical, cheap).
    // Advanced once per block; voices, master-bus targets and rings all read the same snapshot,
    // so display == audio by construction. Free-running: never reset (see DSP/ChaosLorenz.h).
    ChaosLorenz chaos;
    std::atomic<float> chaosDisplay[2] {};   // [0]=X, [1]=Y — published each block (0 when off)
    // Story 22.2: the keyboard's left-hand controls, read from the MIDI buffer in processBlock
    // (channels 1..11). Where the hardware stands, not patch state: nothing resets them.
    float pitchBend = 0.0f;                  // -1..1, 0 = centre; audio thread only
    std::atomic<float> modWheel { 0.0f };    // CC 1, 0..1 — the ModWheel matrix source (voices, master bus, rings)
    float modWheelBus = 0.0f;                // its one-pole copy for the block-rate master-bus targets; audio thread only
    std::atomic<bool> recentreBend { false };   // message thread (controllerInputsChanged) → processBlock: bend back to centre
    // Host gain (AD-13): a fixed pad equal to VOL's default, so a patch has the level in a DAW that
    // it has in the standalone at VOL 0.5 (see processBlock, master stage).
    static constexpr float kHostPad = 0.5f;
    float prevMasterGain = 0.0f;   // last block's applied master gain — ramp target so LFO-modulated
                                   // MASTER · VOL doesn't zipper (block-rate global modulation)
    // Story 21.8: each layer's STEP SEQ + ARP (one lane per layer, see PatternLane). Bound in the
    // constructor: lane 0 = layer A (unprefixed ids, channel 14), lane 1 = layer B ("b", 13).
    std::array<PatternLane, PatternStore::kStepLanes> lanes;
    // One lane's sequencer block, in two halves around the chord filter (processBlock): laneInput
    // reads the RAW keys that reach its layer (root, latch, host anchor, quantised entry); laneOutput
    // writes its figure or arpeggio on the lane's channel and handles the switch-off release.
    void laneInput  (PatternLane& lane, const juce::MidiBuffer& rawMidi, const PatternLane::KeyScope& keys,
                     double syncBpm, bool hosted, bool hostPlaying, const juce::Optional<double>& hostPpq);
    void laneOutput (PatternLane& lane, juce::MidiBuffer& midi, const PatternLane::KeyScope& keys, int numSamples,
                     double syncBpm, bool hosted, bool hostPlaying, const juce::Optional<double>& hostPpq, bool hostRunning);
    // Story 16.1: PERC is the one sound source that is NOT a voice. It renders into the summed mix
    // (after the synth, before the compressor) because as MIDI its hits would be dragged through a
    // layer's filter and effects. See PercSequencer.h.
    PercSequencer perc;
    PatternStore patterns;   // AD-14 (18.5): the PERC grid and the STEP SEQ figures (one per layer, 21.8)
#if JASS_HOSTSYNC_DIAG
    struct DiagRec { double t; double ppq; double stepsPos; double anchor, percAnchor; int playing, seqOn, latch, percOn, kitPending, kitIdx, percStep; int preIdx, preCnt, postIdx, postCnt, postDelay, fired, n; };
    double diagLastAnchor = -2.0;
    int    diagLastState = -1;      // percOn | kitPending | playing | seqOn bits, a change starts a burst
    void   diagNote (const char* what);   // one free-text line (message thread events)
    std::array<DiagRec, 256> diag {};
    std::atomic<int> diagWrite { 0 };
    int  diagRead = 0;
    int  diagBlocksLeft = 0;
    bool diagWasPlaying = false;
    int  diagSeqPre[2]  = { -1, -1 };       // lane A's clock before / after the host sync (laneOutput)
    int  diagSeqPost[3] = { -1, -1, -1 };
    void flushDiag();
#endif
    std::atomic<int> percStepDisplay { 0 };   // playhead for the grid (message thread reads it)
    // 18.6: the host position (beats) each sequencer's step 0 is anchored to, -1 = none yet (the
    // STEP SEQs keep theirs in their lane). PLAY sets it so that the phase CONTINUES where STOP left
    // it (hostLastPpq remembers the last playing position); a channel-1 note entering from silence
    // while the host plays moves the figure's anchor to the note. Session state, not project state:
    // a reopened project starts at step 0 on its first PLAY. See processBlock.
    std::atomic<double> percAnchorPpq { -1.0 };
    double hostLastPpq = -1.0;                    // last position seen while the host played (audio thread)
    bool hostWasPlaying = false;                  // the PLAY edge (audio thread)
    std::atomic<int> percAuditionLane { -1 }; // grid click => sound this lane once (consumed per block)
    std::atomic<bool> percHeld { false };     // STOP latch: pattern paused, see holdPerc
    // True while a preset's kit is still being fetched. PERC stays SILENT until it lands: the KIT
    // index still points at whatever set sits there, and playing a random one — a drum loop, a
    // piano — is worse than playing nothing (maintainer heard exactly that, 2026-08-11).
    std::atomic<bool> percKitPending { false };
    juce::MidiBuffer arpKeptScratch;       // RT-safety (11.1): reused per block instead of a fresh
                                           // MidiBuffer (the chord filter); glide's lives in the engine
    bool autoNoteOn = false;
    std::atomic<bool> pluckRequested { false };   // set by pluckString(), consumed in processBlock

    // Auto-play drone is automatic now: ON until the user plays a key, back ON
    // when a sound generator is (re)activated. No user-facing parameter. The drone
    // lives on its own MIDI channel so it never collides with played notes
    // (kDroneChannel is declared public above — the editor has to recognise it too).
    static constexpr int kDroneNote = 60;        // C4 → transpose ratio 1.0
    std::atomic<bool> autoPlayEnabled { true };
    unsigned prevSourcesMask = 0;                // for rising-edge "generator enabled" detection

    std::atomic<float> currentNoteRatio { 1.0f };           // played note vs C4 (FREQ display); 1.0 = base
    std::atomic<std::uint64_t> heldNotesLo { 0 }, heldNotesHi { 0 };  // bitset of held user notes

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNote, float) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNote, float) override;

    // Shared live-state persistence (see PresetIO::liveStateFile)
    juce::String currentPresetName { "Init" };   // restored from LiveState on start
    std::atomic<bool> liveDirty { false };
    std::vector<float> cleanSnapshot;             // param values at last load/save (empty = "modified")
    uint32_t cleanPercRevision = 0;               // PatternStore::perc revision at the same moment (AD-14)
    std::array<uint32_t, PatternStore::kStepLanes> cleanStepRevision {};   // ...and each STEP SEQ figure's
    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { liveDirty = true; }
    void saveLiveState();

    // Epic 5: keep the two MIX MODE source selectors distinct (a==b would be a no-op / needs
    // self-FM which is a separate future feature). When one is set equal to the other, bump the
    // other to a free OSC. Registered for mixSrcA/mixSrcB only.
    void parameterChanged(const juce::String& paramId, float newValue) override;
    std::atomic<bool> fixingMixSrc { false };   // reentrancy guard (shared by the CROSS-MOD couplings)

    // RT-safety (11.1): APVTS calls parameterChanged SYNCHRONOUSLY on whatever thread changes the
    // param — the AUDIO thread under host automation. The auto-enable couplings below allocate
    // (std::set/map/String) and call setValueNotifyingHost re-entrantly, which must NOT happen on the
    // audio thread. So on the audio thread we only set an atomic dirty flag (alloc/lock-free) and
    // defer the work to the message thread via reconcileParamCouplingsIfDirty() (driven by the
    // editor timer). UI edits stay on the message thread and run synchronously as before, so the
    // maps are only ever touched on the message thread (no data race).
public:
    void reconcileParamCouplingsIfDirty();   // message thread only (editor timer)

    // Preset load in progress (PresetIO::setPresetLoading hook). A saved preset is a snapshot of
    // the state AFTER every coupling already ran, so it must be applied VERBATIM: while this is
    // set, parameterChanged runs none of the couplings below — applyVar's factory-reset plus the
    // file's values would otherwise fire them ~700 times against half-applied intermediate states.
    // Entering a load also clears the two auto-enable memories: they belong to the OUTGOING patch,
    // and carrying them across made the result of a load depend on what was loaded before (the
    // "load it two or three times until it fits" report, 2026-08-23). Message thread only.
    void setPresetLoading (bool loading)
    {
        presetLoading.store (loading);
        if (loading)
        {
            matrixAutoEnabled.clear();
            crossModAutoEnabled.clear();
            matrixEnablesDirty.store (false);   // nothing deferred may fire into the new patch
            crossModDirty.store (false);
            seqArpDirty.store (false);
        }
        // No voice survives a load — kill on BOTH edges. On entry: the outgoing patch's voices,
        // whose note-off may never arrive (the chord filter drops channel 1 while a sequencer
        // runs) — a held voice through new parameters was the "something dirty runs along"
        // report (2026-08-24). On exit: anything that started DURING the load — audio blocks run
        // concurrently with the ~700 parameter writes, and a half-applied state can rising-edge
        // the auto-play drone into a fresh voice. A latched figure is safe either way: its first
        // note enters quantised to the drums' next bar, after this flag is consumed.
        // 1 = every voice; 2 = every voice but layer B's (a single-layer LOAD keeps B and its running
        // figure, review 2026-11-24). A pending "all" is never weakened to "all but B".
        const int want = loadKeepsLayerB ? 2 : 1;
        int expected = 0;
        if (! killVoicesRequested.compare_exchange_strong (expected, want) && want == 1)
            killVoicesRequested.store (1);
    }
    // PresetIO hook (loadPreset): the load in progress keeps layer B. Message thread.
    void setLoadKeepsLayerB (bool keep) { loadKeepsLayerB = keep; }
private:
    std::atomic<bool> presetLoading { false };
    std::atomic<int>  killVoicesRequested { 0 };       // 0 / 1 all / 2 all but B — consumed at the top of processBlock
    bool              loadKeepsLayerB = false;         // message thread: the running load keeps layer B
    std::atomic<bool> matrixEnablesDirty { false };
    std::atomic<bool> crossModDirty      { false };
    std::atomic<bool> seqArpDirty        { false };   // Story 15.1: ARP/STEP SEQ exclusion
    // 21.8: what was just switched on, one bit per lane and module (bit lane*2 + 1 = STEP SEQ, + 0 =
    // ARP) — a mask, so two layers switched before the deferred reconcile both get their exclusion.
    std::atomic<std::uint32_t> pendingExclusive { 0 };
    bool laneBSoundingLast = false;   // audio thread: B's figure started / stopped => matrix claims change
    std::atomic<int>  pendingCrossModCode { -1 };   // encodes which CROSS-MOD param changed (alloc-free)
    void applyCrossModCoupling(const juce::String& paramId);   // the message-thread coupling body
    void applySeqArpExclusion();   // Story 15.1: ARP and STEP SEQ cannot both replace the chord

    // CROSS MOD enable coupling: keep the two operand OSCs enabled while CROSS MOD is on (auto-on
    // with memory), and switch CROSS MOD off when a used operand OSC is turned off. Mirrors the
    // MOD-MATRIX auto-enable. matrixAutoEnabled's sibling for the two OSC operands.
    void syncCrossModEnables(const juce::String& changedParamId, const juce::String& layerPrefix);
    std::map<juce::String, bool> crossModAutoEnabled;   // oscOn id -> true if CROSS MOD switched it on

    // Keep the matrix's SOURCE and TARGET modules in step with the routing: an active slot
    // (DEST != Off) needs both its source module (LFO/Envelope) AND its target module (e.g. the
    // FILTER for Cutoff/Resonance, FORMANT for Vowel, …) enabled, else the modulation is
    // inaudible. Turn each claimed module ON, and turn it back OFF when the LAST slot using it
    // drops — but only if WE auto-enabled it (it was off before and isn't wired elsewhere). A
    // module on for any other reason (user toggle, ADSR default) is never auto-disabled.
    // Targets Pitch/Amplitude are global (no module). Re-evaluated on every slot SRC/DEST change.
    void updateMatrixModuleEnables();
    std::map<juce::String, bool> matrixAutoEnabled;   // enable-param id -> true if auto-enable turned it on

    // AD-16 / Story 21.6 — couplings for both layers, linked editing (focus A+B), RANDOM by focus.
    juce::StringArray listenedParamIds() const;
    void mirrorToLayerB (const juce::String& idA, float deltaRaw);
    void randomizeLayerA();
    std::unordered_map<juce::String, int> linkIndex;   // layer-A patch param id -> slot in linkLastA (built once)
    std::vector<std::atomic<float>> linkLastA;         // A's last value per patch param (raw units; any thread)
    bool linkSuspended = false;                        // RANDOM / bulk writes: nothing to mirror

    // Story 22.1 — the CC path. Audio thread: captureControllers stores the latest value per
    // (channel, CC) and sets a dirty bit — no allocation, lock, String or parameter write. Message
    // thread: controllerPoll (30 Hz) drains the dirty bits, resolves them through the active
    // profile and sets the parameters; every ~2 s it re-reads the enabled MIDI inputs and the
    // Controllers folder. Channels 12..16 are JASS's own and are never captured.
    //
    // Story 22.5: capture also tracks the controller's Shift (its CC is published by the message
    // thread) and keeps, for every (channel, CC), one value slot + dirty bit for "Shift held" and
    // one for "not held" — so a short Shift press inside one poll tick cannot turn a bank move
    // into a module jump, and capture need not know which CC is the selector. A relative encoder
    // sends one message per detent: `ccRelative` sums one step per message (its direction) so none is lost between
    // two ticks. The MiniLab's program-change SysEx releases Shift, in buffer order.
    static constexpr int kCcChannels = ControllerProfiles::kFirstReservedChannel - 1;   // 1..11
    static constexpr size_t kCcSlots = (size_t) kCcChannels * 128;                      // per Shift slot
    static constexpr size_t kCcWords = (size_t) kCcChannels * 2;                        // 128 dirty bits per channel
    std::array<std::atomic<std::uint8_t>, kCcSlots> ccValue {};                        // the latest value, either slot
    std::array<std::atomic<std::uint8_t>, kCcSlots * 2> ccSlotValue {};                // [not held | Shift held]
    std::array<std::atomic<std::uint64_t>, kCcWords * 2> ccDirty {};                   // [not held | Shift held]
    std::array<std::atomic<std::int32_t>, kCcSlots> ccRelative {};                     // summed steps (+1 / -1 per message)
    // Button presses (messages with value >= 64) per CC since the last poll: press and release
    // inside one tick leave the latest value at 0, the count still says "pressed once" ("@power").
    std::array<std::atomic<std::int32_t>, kCcSlots> ccPresses {};
    // (channel << 8) | cc of each "@shift" entry of the active profile, channel 0 = any; -1 = unused.
    static constexpr size_t kShiftCcs = 4;
    std::array<std::atomic<int>, kShiftCcs> ccShiftCc { -1, -1, -1, -1 };
    std::atomic<bool> ccShiftHeld { false };  // audio thread (the CC, the SysEx) and message thread (resets)
    void captureControllers (const juce::MidiBuffer& midi) noexcept;                   // audio thread
    struct ControllerPoll : juce::Timer
    {
        std::function<void()> tick;
        void timerCallback() override { if (tick) tick(); }
    };
    ControllerPoll controllerPoll;
    std::vector<ControllerProfiles::Profile> controllerProfiles;   // message thread only
    int controllerActive = -1;                                    // index into controllerProfiles
    juce::String controllerHandPick, controllerFolderSig;
    juce::StringArray controllerInputs;                           // enabled MIDI inputs last seen
    int controllerTicks = 0, controllerRevision = 0;
    void pollControllers();                                       // message thread, 30 Hz
    void rescanControllers (bool force);                          // inputs + folder, re-pick
    // Story 22.5 (message thread only).
    const ControllerProfiles::Profile* activeControllerProfile() const;
    void controllerProfilePicked();                               // after every re-pick: publish Shift CC, gate the selection
    void controllerInputsChanged();                               // a lost Shift release must not stick; control positions are unknown again
    void applyControllerValue (juce::RangedAudioParameter& p, float v01, bool pinned);
    // Story 22.8 — soft takeover. Per (channel, CC), message thread only: where the control stood
    // at its last message (-1 = not known yet: its first message moves nothing), its travel towards
    // the next step of a stepped target (CC units), and what it last asked of a parameter that
    // snapped (`wanted` vs. the value `written`), so small moves are not lost to the snapping.
    // The travel belongs to one stepped target in one state (`stepParam` on item `stepIndex`): a
    // new target, or an index somebody else changed, starts it afresh - what was turned on one
    // module's switch must not flip the next one's. `scaleRef` / `scaleDir`: where a control with
    // ends last really stood and which way it went (ControllerProfiles::scaleReference).
    struct ControllerTouch
    {
        int last = -1;
        float travel = 0.0f;
        const juce::RangedAudioParameter* param = nullptr;
        float wanted = 0.0f, written = 0.0f;
        const juce::RangedAudioParameter* stepParam = nullptr;
        int stepIndex = -1;
        int scaleRef = -1, scaleDir = 0;
    };
    std::array<ControllerTouch, kCcSlots> controllerTouch {};
    // The MiniLab changed its program: each program has its own CCs, so the positions noted for
    // the other one may be stale. Set at capture (audio thread), taken by the poll.
    std::atomic<bool> ccForgetPositions { false };
    // A stepped target: item `index` of `count` -> the item the control moves it to (-1: none).
    int stepControllerParam (size_t at, const juce::RangedAudioParameter& p, ControllerProfiles::Takeover mode,
                             int index, int count, int last, int now);
    // One hardware control moves one parameter by its takeover mode (`last` / `now`: CC values).
    void moveControllerParam (juce::RangedAudioParameter& p, ControllerProfiles::Takeover mode, size_t at, int last, int now, bool pinned);
    void applyModuleControl (const ControllerProfiles::ModuleControl& mc, ControllerProfiles::Takeover mode, size_t at, int last, int now);
    const ControllerFrame* selectedControllerFrame() const;
    void setControllerSelection (const juce::String& entryId, bool layerB, int bank);
    std::vector<ControllerFrame> controllerFrames;                // selectable frames on screen, reading order
    ControllerSelection controllerSel;                            // bankCount is filled by the getter
    int controllerSelRevision = 0;
    int controllerBankRef = -1;                                   // the selector's last value seen under Shift; -1 = none yet
    float controllerBankTravel = 0.0f;                            // its travel since the last bank step, in CC units
    static int controllerBankCount (const ControllerFrame& f);    // highest bank of its controls + 1

    JUCE_DECLARE_WEAK_REFERENCEABLE(SynthyProcessor)   // for lifetime-guarding async (callAsync) callbacks
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyProcessor)
};
