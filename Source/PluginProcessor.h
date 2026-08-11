#pragma once
#include <JuceHeader.h>
#include "Audio/SynthVoice.h"
#include "Audio/SynthSound.h"
#include "Audio/Parameters.h"
#include "DSP/WaveformCapture.h"
#include "DSP/StereoWidth.h"
#include "DSP/BinauralRoom.h"
#include "DSP/Compressor.h"
#include "DSP/Arpeggiator.h"
#include "DSP/StepSequencer.h"   // Story 15.1
#include "DSP/PercSequencer.h"   // Story 16.1 — layer B: four percussion tracks on the master bus
#include <vector>
#include <map>

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
    double getTailLengthSeconds() const override { return 0.0; }

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
    // The auto-play drone's own channel (see autoPlayEnabled). Public because the editor has to
    // recognise it: a drone note is not a played key, so it must not be written into a STEP SEQ
    // step while a figure is being recorded (Story 15.4).
    static constexpr int kDroneChannel = 16;

    // While the editor is recording a figure into STEP SEQ (Story 15.4), a played key WRITES a step
    // instead of playing the pattern: the sequencer must not take it as its root, or every entered
    // note would restart and transpose the figure under the writer's hands. Set from the message
    // thread, read once per block — the editor clears it when it stops recording and in its
    // destructor, so a closed window can never leave the sequencer rootless.
    void setSeqRecordArmed (bool armed) { seqRecordArmed.store (armed); }

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

    // Reset everything to defaults, then enable only OSC 1 (auto-play drone).
    void resetToDefault();

    // Pitch ratio of the note currently being played (relative to C4 = note 60);
    // 1.0 when only the drone or nothing sounds. Used by the editor so the OSC
    // FREQ knobs can display the actually-played frequency.
    float getCurrentNoteRatio() const { return currentNoteRatio.load(); }

    // Which step PERC is on, for the grid's playhead (Story 16.1). Plain atomic read.
    int getPercStep() const { return percStepDisplay.load(); }
    // MIDI note the STEP SEQ is sounding, or -1. For the on-screen keyboard only (see seqNoteDisplay).
    int getSeqNote() const { return seqNoteDisplay.load(); }

    // Move the LATCHED sequencer root (see seqLatchedRoot). The pattern keeps running after the key
    // is released, so an octave shift has no held note left to move — the editor sends the ±12 here
    // instead, and the figure follows the octave keys exactly as a held key would have.
    void transposeSeqLatch (int semitones)
    {
        const int r = seqLatchedRoot.load();
        if (r >= 0)
            seqLatchedRoot.store (juce::jlimit (0, 127, r + semitones));
    }
    // Is a latched figure running? (SPACE stops it — the editor needs to know whether there is
    // anything to stop before it falls through to the Karplus pluck.)
    bool isSeqLatched() const { return seqLatchedRoot.load() >= 0; }
    // Stop the latched figure. Clearing the root is enough: the next block sees nothing playing,
    // releases the sounding note and re-arms the pattern at step 0, exactly as letting go of the
    // key did before the latch existed.
    void stopSeqLatch() { seqLatchedRoot.store (-1); }
    // Start a latched figure without a key — used when a preset whose STEP SEQ is ON is loaded, so
    // the patch plays itself the moment it arrives instead of waiting to be touched. It also ends
    // the auto-play drone: the instrument IS playing now, and hearing a held C4 under a bass figure
    // is exactly the confusion this is meant to remove.
    void startSeqLatch (int midiNote)
    {
        seqLatchedRoot.store (juce::jlimit (0, 127, midiNote));
        autoPlayEnabled.store (false);
    }

    // Sound one PERC lane once — the grid plays a step as it is placed. Message thread → audio
    // thread through one atomic; the block consumes it.
    void auditionPercLane (int lane) { percAuditionLane.store (lane); }

    // Current LFO oscillation value (-1..+1, already scaled by depth) for the
    // editor's live modulation rings. Driven by a dedicated display LFO that
    // mirrors the patch's LFO params and runs even when no note sounds.
    float getLfoDisplayValue(int i) const { return lfoDisplayValues[i].load(); }   // per-LFO (for the rings)

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
    void restoreModifiedState(bool modified);     // on LiveState load: clean baseline, or force "modified"

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

    private:
        struct Request { juce::String name, target; int gen = 0; };
        Request takeRequest();
        void loadRequested (const Request& r);
        SynthyProcessor& owner;
        juce::CriticalSection requestLock;
        std::vector<Request> queue;             // at most one entry per selector
        int genSampler = 0, genPerc = 0;        // guarded by requestLock
    };
    SamplePreloadThread samplePreload { *this };

    // Select a sample set by index (message thread). Used by the loader once a requested set
    // has arrived; `generation` guards against a preset that was loaded in the meantime.
    void selectSamplerSet (int index, int generation, const juce::String& targetParamId);

    juce::Synthesiser synth;
    // Flat list of the synth's voices, built once in the constructor and handed to every voice so a
    // note-on can reach its siblings (Story 12.7 choke groups). Non-owning — the synthesiser owns
    // them — and never mutated afterwards, so reading it on the audio thread needs no lock.
    std::vector<SynthVoice*> voiceRoster;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    // Snapshot length = the LONGEST scope window (100 ms) at the highest sample rate we
    // expect (96 kHz): 0.100 s × 96000 = 9600. The scope shows only the first N samples of
    // this for the selected window, so a too-small buffer would clamp the long windows to
    // identical traces. Sized once here (never reallocated) so the GUI's updateSnapshot never
    // races a resize. (Spectrum reads at most fftSize=1024 of it, zero-padding when shorter.)
    WaveformCapture waveformCapture { 9600 };
    StereoWidth stereoWidth;   // final pseudo-stereo stage (mono engine -> stereo)
    BinauralRoom binauralRoom; // Kunstkopf externalization: shared early-reflection stage (Story 10.4)
    double samplerMasterFrac = 0.0;   // Story 12.1: shared SAMPLER loop phase (see processBlock)
    bool wasKunstkopf = false; // mode-transition edge -> reset the reflection ring (no stale audio)
    Compressor  compressor;    // master-bus compressor (runs on the summed mix, before width)
    LFO uiLfos[kNumLFOs];      // display-only LFOs mirroring each patch LFO (for the rings)
    std::atomic<float> lfoDisplayValues[kNumLFOs] {};
    float prevMasterGain = 0.0f;   // last block's applied master gain — ramp target so LFO-modulated
                                   // MASTER · VOL doesn't zipper (block-rate global modulation)
    Arpeggiator arp;
    StepSequencer stepSeq;   // Story 15.1 — shares the ARP's note-replacement slot in processBlock
    // Story 16.1: PERC is the one sound source that is NOT a voice. It renders into the summed mix
    // (after the synth, before the compressor) because JASS is monotimbral: as MIDI its hits would
    // be dragged through the patch's filter and effects. See PercSequencer.h.
    PercSequencer perc;
    bool seqKeyWasHeld = false;   // edge detect: the moment a figure starts from silence, so its
                                  // entry can be quantised to the drum pattern (16.1 AC6)
    std::atomic<bool> seqRecordArmed { false };   // see setSeqRecordArmed (Story 15.4)
    std::atomic<int> percStepDisplay { 0 };   // playhead for the grid (message thread reads it)
    std::atomic<int> seqNoteDisplay { -1 };   // note the STEP SEQ holds, for the on-screen keyboard
    std::atomic<int> seqLatchedRoot { -1 };   // STEP SEQ latch: the root outlives the key (see below)
    std::atomic<int> percAuditionLane { -1 }; // grid click => sound this lane once (consumed per block)
    // True while a preset's kit is still being fetched. PERC stays SILENT until it lands: the KIT
    // index still points at whatever set sits there, and playing a random one — a drum loop, a
    // piano — is worse than playing nothing (maintainer heard exactly that, 2026-08-11).
    std::atomic<bool> percKitPending { false };
    std::vector<int> arpHeldScratch;   // reused per block (no RT realloc)
    juce::MidiBuffer arpKeptScratch;       // RT-safety (11.1): reused per block instead of a fresh
    juce::MidiBuffer glideRebuiltScratch;  // MidiBuffer (arp filter + mono-glide rebuild)

    // Poly-glide (portamento): the processor assigns each newly-started note a predecessor
    // pitch to glide FROM (pitch-sorted against the previous chord) and shares it with all
    // voices via glideInfo. Held-note tracking excludes the auto-play drone.
    GlideInfo glideInfo;
    std::vector<int> glideHeld;        // notes currently held down
    std::vector<int> glideLastChord;   // last non-empty held set (glide source after a gap)
    std::vector<int> glideNewNotes, glideOffNotes;   // per-block scratch (no RT realloc)
    int monoSounding = -1;             // Mono glide: the note currently sounding (last-note priority)
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
private:
    std::atomic<bool> matrixEnablesDirty { false };
    std::atomic<bool> crossModDirty      { false };
    std::atomic<bool> seqArpDirty        { false };   // Story 15.1: ARP/STEP SEQ exclusion
    std::atomic<bool> pendingExclusiveIsSeq { false };// which of the two was just switched on
    std::atomic<int>  pendingCrossModCode { -1 };   // encodes which CROSS-MOD param changed (alloc-free)
    void applyCrossModCoupling(const juce::String& paramId);   // the message-thread coupling body
    void applySeqArpExclusion();   // Story 15.1: ARP and STEP SEQ cannot both replace the chord

    // CROSS MOD enable coupling: keep the two operand OSCs enabled while CROSS MOD is on (auto-on
    // with memory), and switch CROSS MOD off when a used operand OSC is turned off. Mirrors the
    // MOD-MATRIX auto-enable. matrixAutoEnabled's sibling for the two OSC operands.
    void syncCrossModEnables(const juce::String& changedParamId);
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

    JUCE_DECLARE_WEAK_REFERENCEABLE(SynthyProcessor)   // for lifetime-guarding async (callAsync) callbacks
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyProcessor)
};
