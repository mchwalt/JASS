// JASS_Render — headless offline renderer for null tests (Story 21.4, S0 of AD-16).
//
//   JASS_Render.exe <preset.jass> <out.wav> [seconds] [options]
//   (preset "init" = the Init patch, as RESET leaves it — both layers)
//
//   --b <preset.jass>   AD-16 (21.5): load that preset's patch into layer B and turn B on (LAYER)
//   --split <note>      SPLIT at <note> instead of LAYER (below → B)
//   --solo a|b          mute the other layer (its LEVEL to 0) — compare one engine alone
//   --roundtrip         after loading: preset (toVar → applyVar) and DAW state (get/setState) into a
//                       fresh processor; every parameter must come back identical (exit 3 if not)
//   --note <n>          play only note n (held 0.35 s, three times) instead of the fixed script
//   JASS_Render.exe --selftest   router / Split filter / pattern channel / layer-B preset checks
//                                (run from the repo root: it reads DemoPresets), exit 4 on a failure
//   --over-init         start from Init, then load as the app does with A+B lit (the whole file)
//   --save <file.jass>  write the loaded state back as a preset (round-trip a file byte for byte)
//
// Loads ONE preset through the normal PresetIO path into a fresh processor, waits for the sample
// sets the preset asks for, renders a fixed MIDI script at 48 kHz in 512-sample blocks and writes
// a 32-bit float stereo WAV. One preset per process on purpose: random sources seed from
// process-wide instance counters (LFO.h), so a fresh process per file is what makes two builds
// comparable sample for sample. Compare two output folders with tools/rig/nulltest.py.
//
// The processor runs with an undefined wrapper type, i.e. the host path: it never reads or writes
// the standalone LiveState, never runs the startup preset conversion, and starts from Init before
// the preset lands. A preset older than the current format is migrated IN PLACE by PresetIO (with
// a backup) — the driver therefore hands in copies, never the user's files.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Audio/PresetIO.h"

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlock      = 512;

    struct Ev { double t; int note; float vel; bool on; };

    // The fixed MIDI script. Covers: a single note, a chord starting mid-block, more notes than
    // voices (stealing), a long silence (voices go idle, then a voice is re-used), legato steps
    // (glide), and velocity spread. Times in seconds; channel 1.
    std::vector<Ev> midiScript (int singleNote = -1)
    {
        std::vector<Ev> e;
        auto note = [&e] (double on, double off, int n, float v) { e.push_back ({ on, n, v, true }); e.push_back ({ off, n, 0.0f, false }); };
        if (singleNote >= 0)   // --note: one note, held 0.35 s, three times (a bass hit to measure)
        {
            for (int i = 0; i < 3; ++i)
                note (0.10 + i * 1.0, 0.45 + i * 1.0, singleNote, 0.80f);
            return e;
        }
        note (0.10, 0.90, 60, 0.80f);                                   // single note
        note (1.0047, 2.20, 48, 0.60f); note (1.0047, 2.20, 52, 0.70f); // chord, mid-block start
        note (1.0047, 2.20, 55, 0.90f);
        for (int i = 0; i < 11; ++i)                                    // 11 staggered notes > 8 voices
            note (2.50 + i * 0.05, 4.20, 50 + i * 3, 0.30f + 0.06f * (float) i);
        note (6.50, 7.30, 69, 1.00f);                                   // re-used after a long idle
        for (int i = 0; i < 8; ++i)                                     // legato line (glide)
            note (7.80 + i * 0.12, 7.80 + i * 0.12 + 0.14, 57 + ((i * 5) % 12), 0.75f);
        std::stable_sort (e.begin(), e.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
        return e;
    }

    struct Options
    {
        double     seconds = 10.0;
        juce::File layerB;           // empty = B stays as the preset left it
        int        split   = -1;     // -1 = LAYER
        int        solo    = 0;      // 0 = both, 1 = A only, 2 = B only
        bool       roundtrip = false;
        juce::File saveTo;
        bool       overInit = false; // load like the app with A+B lit, on top of Init (LoadScope::Whole)
        int        note     = -1;    // --note: play only this note instead of the fixed script
    };

    void setParam (SynthyProcessor& p, const juce::String& id, float raw)
    {
        if (auto* prm = p.getAPVTS().getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (raw));
    }

    // A MOD MATRIX slot whose MODULE is Off is factory state: the preset leaves it out on purpose
    // and the survivors compact upward on reload (PresetIO), so its other fields do not round-trip.
    bool isUnusedMatrixSlotParam (SynthyProcessor& p, const juce::String& id)
    {
        const juce::String bp (Modules::kLayerBPrefix);
        const juce::String prefix = id.startsWith (bp + "modSlot") ? bp : juce::String();
        const auto rest = id.substring (prefix.length());
        if (! rest.startsWith ("modSlot")) return false;
        const int n = rest.substring (7).getIntValue();
        if (n < 1) return false;
        auto* module = p.getAPVTS().getRawParameterValue (prefix + "modSlot" + juce::String (n) + "Module");
        return module != nullptr && *module < 0.5f;
    }

    // Every parameter of `b` equals `a`'s (by id). Returns the number of mismatches, prints a few.
    int compareParams (SynthyProcessor& a, SynthyProcessor& b, const char* what)
    {
        int bad = 0;
        for (auto* p : a.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && ! isUnusedMatrixSlotParam (a, rp->paramID))
                if (auto* q = b.getAPVTS().getParameter (rp->paramID); q == nullptr || q->getValue() != rp->getValue())
                {
                    if (bad++ < 5)
                        std::cerr << "  " << what << " mismatch: " << rp->paramID << " " << rp->getValue()
                                  << " vs " << (q != nullptr ? q->getValue() : -1.0f) << std::endl;
                }
        return bad;
    }

    class Job : private juce::Timer
    {
    public:
        Job (juce::File presetIn, juce::File wavOut, Options o)
            : preset (std::move (presetIn)), out (std::move (wavOut)), opt (std::move (o)), seconds (opt.seconds)
        {
            juce::PluginHostType::jucePlugInClientCurrentWrapperType = juce::AudioProcessor::wrapperType_Undefined;
            proc = std::make_unique<SynthyProcessor>();
            proc->setRateAndBufferSizeDetails (kSampleRate, kBlock);
            proc->prepareToPlay (kSampleRate, kBlock);

            if (preset.getFileName() == "init")   // the Init patch itself (RESET), no file
                proc->resetToDefault();
            else if (opt.overInit)
            {
                proc->resetToDefault();
                if (! PresetIO::loadPreset (proc->getAPVTS(), preset, PresetIO::LoadScope::Whole).ok)
                { fail ("cannot load preset " + preset.getFullPathName()); return; }
            }
            else if (const auto res = PresetIO::loadFromFile (proc->getAPVTS(), preset); ! res.ok)
            { fail ("cannot load preset " + preset.getFullPathName()); return; }
            if (opt.layerB != juce::File())
            {
                if (! PresetIO::loadIntoLayerB (proc->getAPVTS(), opt.layerB).ok)
                { fail ("cannot load into layer B (format older than current?) " + opt.layerB.getFullPathName()); return; }
                setParam (*proc, Parameters::ID::layerMode, 1.0f);   // DUAL
            }
            if (opt.split >= 0)
            {
                setParam (*proc, Parameters::ID::layerMode, 2.0f);   // SPLIT
                setParam (*proc, Parameters::ID::splitNote, (float) opt.split);
            }
            if (opt.solo == 1) setParam (*proc, Parameters::ID::layerBLevel, 0.0f);
            if (opt.solo == 2) setParam (*proc, Parameters::ID::layerALevel, 0.0f);
            proc->requestGlobalLfoRearm();          // what the editor does after a load (20.1)
            started = juce::Time::getMillisecondCounterHiRes();
            startTimer (50);
        }

        int exitCode = 0;

    private:
        void fail (const juce::String& msg)
        {
            std::cerr << "JASS_Render: " << msg << std::endl;
            exitCode = 1;
            juce::MessageManager::getInstance()->stopDispatchLoop();
        }

        void timerCallback() override
        {
            auto& h = PresetIO::hooks (proc->getAPVTS());
            const bool pending = h.pendingSamplerSetName.isNotEmpty() || h.pendingPercKitName.isNotEmpty()
                              || h.pendingBSamplerSetName.isNotEmpty();
            if (pending && juce::Time::getMillisecondCounterHiRes() - started < 180000.0)
                return;                              // sample sets still arriving
            stopTimer();
            if (pending)
                std::cerr << "JASS_Render: warning, sample set still pending after 180 s" << std::endl;
            if (opt.saveTo != juce::File())
                PresetIO::saveToFile (proc->getAPVTS(), opt.saveTo, PresetIO::nameFromFile (preset));
            if (opt.roundtrip)
                roundtrip();
            render();
            juce::MessageManager::getInstance()->stopDispatchLoop();
        }

        // AC 2 of Story 21.5: the state survives a preset and a DAW-state round trip, layer B included.
        void roundtrip()
        {
            auto& a = proc->getAPVTS();
            int bad = 0;
            {
                auto fresh = std::make_unique<SynthyProcessor>();
                const auto json = juce::JSON::toString (PresetIO::toVar (a, "rt"), false, 12);
                PresetIO::applyVar (fresh->getAPVTS(), juce::JSON::parse (json));
                bad += compareParams (*proc, *fresh, "preset");
            }
            {
                auto fresh = std::make_unique<SynthyProcessor>();
                juce::MemoryBlock state;
                proc->getStateInformation (state);
                fresh->setStateInformation (state.getData(), (int) state.getSize());
                bad += compareParams (*proc, *fresh, "daw-state");
            }
            std::cout << "roundtrip " << (bad == 0 ? "ok" : "FAILED") << " (" << bad << " mismatches)" << std::endl;
            if (bad != 0) exitCode = 3;
        }

        void render()
        {
            const auto script = midiScript (opt.note);
            const int total = (int) std::ceil (seconds * kSampleRate);
            juce::AudioBuffer<float> all (2, total);
            all.clear();
            juce::AudioBuffer<float> block (2, kBlock);
            juce::MidiBuffer midi;
            size_t next = 0;
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            for (int pos = 0; pos < total; pos += kBlock)
            {
                const int n = std::min (kBlock, total - pos);
                block.setSize (2, n, false, false, true);
                block.clear();
                midi.clear();
                while (next < script.size())
                {
                    const auto s = (int) std::llround (script[next].t * kSampleRate);
                    if (s >= pos + n) break;
                    const auto& ev = script[next++];
                    midi.addEvent (ev.on ? juce::MidiMessage::noteOn (1, ev.note, ev.vel)
                                         : juce::MidiMessage::noteOff (1, ev.note), juce::jmax (0, s - pos));
                }
                proc->processBlock (block, midi);
                for (int c = 0; c < 2; ++c)
                    all.copyFrom (c, pos, block, c, 0, n);
            }
            const double renderMs = juce::Time::getMillisecondCounterHiRes() - t0;   // CPU hint, not part of the null

            out.deleteFile();
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (out);
            juce::WavAudioFormat wav;
            auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                           .withSampleRate (kSampleRate)
                                                           .withNumChannels (2)
                                                           .withBitsPerSample (32)
                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            if (writer == nullptr) { fail ("cannot write " + out.getFullPathName()); return; }
            writer->writeFromAudioSampleBuffer (all, 0, total);
            std::cout << out.getFileName() << "  peak " << juce::Decibels::gainToDecibels (all.getMagnitude (0, total), -200.0f)
                      << " dBFS  render " << juce::String (renderMs, 0) << " ms" << std::endl;
        }

        juce::File preset, out;
        Options opt;
        double seconds;
        double started = 0.0;
        std::unique_ptr<SynthyProcessor> proc;
    };
}

namespace
{
    // --selftest (code review 2026-11-18): the routing the null test cannot see — all 24 presets
    // are Single / layer A, so the router, the Split key filter and the pattern channel never run
    // there. Asserts destinations and "no key left down"; exit 4 on the first failure.
    int fails = 0;
    void expect (bool ok, const char* what)
    {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
        if (! ok) ++fails;
    }

    void routerChecks()
    {
        std::cout << "LayerRouter" << std::endl;
        LayerRouter r;
        LayerRouter::Settings s;
        s.playA = s.playB = true; s.split = true; s.splitNote = 60;
        s.patternChannel = SynthyProcessor::kPatternChannel;
        s.droneChannel = SynthyProcessor::kDroneChannel; s.auditionChannel = SynthyProcessor::kAuditionChannel;
        juce::MidiBuffer in, a, b;
        auto one = [&] (const juce::MidiMessage& m) { in.clear(); in.addEvent (m, 0); r.route (in, a, b, s); };
        one (juce::MidiMessage::noteOn (1, 59, 0.8f));  expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "split 60: key 59 -> B only");
        one (juce::MidiMessage::noteOn (1, 60, 0.8f));  expect (a.getNumEvents() == 1 && b.getNumEvents() == 0, "split 60: key 60 -> A only");
        s.splitNote = 40;   // the split moves while both keys are held
        one (juce::MidiMessage::noteOff (1, 59));       expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "split moved: 59's note-off still -> B");
        one (juce::MidiMessage::noteOff (1, 60));       expect (a.getNumEvents() == 1 && b.getNumEvents() == 0, "split moved: 60's note-off still -> A");
        s.splitNote = 60; s.patternDest = LayerRouter::kB;
        one (juce::MidiMessage::noteOn (SynthyProcessor::kPatternChannel, 84, 0.8f));
        expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "pattern note 84 (above the split), TARGET B -> B only");
        s.split = false;   // DUAL, then back to SPLIT while the key is held
        one (juce::MidiMessage::noteOn (1, 70, 0.8f));  expect (a.getNumEvents() == 1 && b.getNumEvents() == 1, "dual: key -> both");
        s.split = true;
        one (juce::MidiMessage::noteOff (1, 70));       expect (a.getNumEvents() == 1 && b.getNumEvents() == 1, "dual -> split while held: note-off -> both");
    }

    struct Rig
    {
        std::unique_ptr<SynthyProcessor> p;
        juce::AudioBuffer<float> buf { 2, kBlock };
        Rig()
        {
            juce::PluginHostType::jucePlugInClientCurrentWrapperType = juce::AudioProcessor::wrapperType_Undefined;
            p = std::make_unique<SynthyProcessor>();
            p->setRateAndBufferSizeDetails (kSampleRate, kBlock);
            p->prepareToPlay (kSampleRate, kBlock);
            p->resetToDefault();
        }
        void set (const char* id, float raw) { setParam (*p, id, raw); }
        // One block with the given events (sample offsets), returns nothing; buffer discarded.
        void block (std::initializer_list<std::pair<int, juce::MidiMessage>> ev = {})
        {
            juce::MidiBuffer m;
            for (const auto& [pos, msg] : ev) m.addEvent (msg, pos);
            buf.clear();
            p->processBlock (buf, m);
        }
        void run (double seconds) { for (int i = 0; i < (int) (seconds * kSampleRate / kBlock); ++i) block(); }
    };

    // Layer B in presets (review group 2): a B that differs from Init survives a save/load, an
    // untouched B is not written, and the LOAD scopes keep or replace B as documented.
    void presetChecks()
    {
        using namespace Parameters;
        const juce::String b (Modules::kLayerBPrefix);
        std::cout << "Presets with layer B" << std::endl;
        auto rawOf = [] (SynthyProcessor& p, const juce::String& id) { return p.getAPVTS().getRawParameterValue (id)->load(); };

        Rig built;   // a layer B of its own: cutoff, a matrix routing, Split
        setParam (*built.p, b + ID::filterCutoff, 1234.0f);
        setParam (*built.p, b + ID::modSlotModule (2), 1.0f);   // a routing in slot 2
        setParam (*built.p, ID::layerMode, 2.0f);
        const auto json = juce::JSON::toString (PresetIO::toVar (built.p->getAPVTS(), "st"), false, 12);
        expect (juce::JSON::parse (json)["LayerB"].isObject(), "a B of its own is written as LayerB");
        // Every parameter but PERC's KIT: a fresh processor may still be fetching its kit in the
        // background, and the file then carries the pending name - a load-timing matter, not B's.
        auto sameState = [] (SynthyProcessor& x, SynthyProcessor& y)
        {
            int bad = 0;
            for (auto* p : x.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && rp->paramID != Parameters::ID::percKit)
                    if (auto* q = y.getAPVTS().getParameter (rp->paramID); q == nullptr || std::abs (q->getValue() - rp->getValue()) > 1.0e-6f)
                        if (bad++ < 5) std::cerr << "        differs: " << rp->paramID << std::endl;
            return bad;
        };
        {
            Rig fresh;
            PresetIO::applyVar (fresh.p->getAPVTS(), juce::JSON::parse (json));
            expect (sameState (*built.p, *fresh.p) == 0, "save -> load: every parameter back, B included");
        }
        {
            Rig init;   // RESET: B carries its Init bass, untouched
            const auto v = PresetIO::toVar (init.p->getAPVTS(), "init");
            expect (! v["LayerB"].isObject(), "B at its Init sound is not written (single-layer file)");
            Rig other;
            setParam (*other.p, b + ID::filterCutoff, 555.0f);   // a different B before the load
            PresetIO::applyVar (other.p->getAPVTS(), juce::JSON::parse (juce::JSON::toString (v, false, 12)));
            expect (sameState (*init.p, *other.p) == 0, "a file without LayerB loads B as its Init sound (LiveState after a restart)");
        }

        // LOAD scopes on a copy of a shipped v10 demo (no LayerB) and on the file written above.
        const auto demo = juce::File::getCurrentWorkingDirectory().getChildFile ("DemoPresets/Matrix Demo.jass");
        if (! demo.existsAsFile()) { expect (false, "DemoPresets/Matrix Demo.jass found (run from the repo root)"); return; }
        const auto tmpOld = juce::File::createTempFile (".jass");
        const auto tmpB   = juce::File::createTempFile (".jass");
        demo.copyFileTo (tmpOld);
        tmpB.replaceWithText (json);
        {
            Rig r;   // the player's B: cutoff 777, Split
            setParam (*r.p, b + ID::filterCutoff, 777.0f);
            setParam (*r.p, ID::layerMode, 2.0f);
            PresetIO::loadPreset (r.p->getAPVTS(), tmpOld, PresetIO::LoadScope::Whole);
            expect (std::abs (rawOf (*r.p, b + ID::filterCutoff) - 777.0f) < 0.5f && juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 2,
                    "A+B load of a file without LayerB keeps B and the mode");
            PresetIO::loadPreset (r.p->getAPVTS(), tmpB, PresetIO::LoadScope::KeepLayerB);
            expect (std::abs (rawOf (*r.p, b + ID::filterCutoff) - 777.0f) < 0.5f, "A-only load of a file WITH LayerB keeps B");
            PresetIO::loadPreset (r.p->getAPVTS(), tmpB, PresetIO::LoadScope::Whole);
            expect (std::abs (rawOf (*r.p, b + ID::filterCutoff) - 1234.0f) < 0.5f, "A+B load of a file with LayerB replaces B");
        }
        tmpOld.deleteFile();
        tmpB.deleteFile();
    }

    void processorChecks()
    {
        using namespace Parameters;
        std::cout << "Split + ARP (TARGET Auto = B): a live tap above the split" << std::endl;
        {
            Rig r;
            r.set (ID::layerMode, 2.0f); r.set (ID::splitNote, 60.0f); r.set (ID::arpOn, 1.0f);
            r.run (0.1);
            r.block ({ { 10, juce::MidiMessage::noteOn (1, 72, 0.8f) }, { 200, juce::MidiMessage::noteOff (1, 72) } });
            r.run (0.5);
            expect (r.p->layerKeysDown (false) == 0, "tap (on + off in one block) leaves no key down in A");
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 48, 0.8f) } });   // the left hand feeds the ARP in B
            r.run (0.5);
            const int bDown = r.p->layerKeysDown (true), aDown = r.p->layerKeysDown (false);
            expect (bDown >= 1 && aDown == 0, "left-hand key: the ARP plays in B, nothing in A");
            r.block ({ { 0, juce::MidiMessage::noteOff (1, 48) } });
            r.run (1.0);
            expect (r.p->layerKeysDown (true) == 0 && r.p->layerKeysDown (false) == 0, "key released: no key left down");
        }
        std::cout << "Mono GLIDE: the forced note-off goes to the sounding note's channel" << std::endl;
        {
            // Directly at an engine: two overlapping pattern-channel notes, GLIDE Mono on. The
            // rebuilt buffer must release the first one ON the pattern channel (the synth matches
            // note-offs by channel; a channel-1 note-off would leave it sounding).
            Rig r;
            r.set (ID::glideOn, 1.0f); r.set (ID::glideMode, 0.0f);
            PatchEngine e (r.p->getAPVTS(), {});
            const int pc = SynthyProcessor::kPatternChannel;
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::noteOn (pc, 60, 0.8f), 0);
            e.processGlide (m, SynthyProcessor::kDroneChannel);
            m.clear();
            m.addEvent (juce::MidiMessage::noteOn (pc, 64, 0.8f), 0);
            e.processGlide (m, SynthyProcessor::kDroneChannel);
            bool offOnPattern = false;
            for (const auto meta : m)
                if (const auto msg = meta.getMessage(); msg.isNoteOff() && msg.getNoteNumber() == 60)
                    offOnPattern = msg.getChannel() == pc;
            expect (offOnPattern, "note 60 released on the pattern channel when 64 starts");
        }
        std::cout << "Host panic on channel 1 stops a latched STEP SEQ note" << std::endl;
        {
            // One long step (free rate 0.5 steps/s = 2 s, gate 1): the figure latches and keeps its
            // note down; only the panic can release it inside the step.
            Rig r;
            auto& st = r.p->getPatterns().step;
            st.setOn (0, true); st.setPitch (0, 0);
            r.set (ID::seqLength, 1.0f); r.set (ID::seqSync, 0.0f); r.set (ID::seqRate, 0.5f); r.set (ID::seqGate, 1.0f);
            r.set (ID::seqOn, 1.0f);
            r.run (0.1);
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 60, 0.8f) } });
            r.block ({ { 0, juce::MidiMessage::noteOff (1, 60) } });   // released: the figure is latched
            r.run (0.3);
            expect (r.p->layerKeysDown (false) >= 1, "figure note sounding (latched) before the panic");
            r.block ({ { 0, juce::MidiMessage::allNotesOff (1) } });
            r.run (0.6);   // allNotesOff tails off (the voice keeps its key flag until it ends) - past the 0.25 s release
            expect (r.p->layerKeysDown (false) == 0, "All-Notes-Off on channel 1: the pattern note has ended (step is 2 s long)");
        }
    }
}

int main (int argc, char* argv[])
{
    if (argc >= 2 && juce::String (argv[1]) == "--selftest")
    {
        juce::ScopedJuceInitialiser_GUI juceInit;
        routerChecks();
        processorChecks();
        presetChecks();
        std::cout << (fails == 0 ? "selftest: all passed" : "selftest: FAILED") << std::endl;
        return fails == 0 ? 0 : 4;
    }
    if (argc < 3)
    {
        std::cerr << "usage: JASS_Render <preset.jass> <out.wav> [seconds] [--b preset] [--split note] "
                     "[--solo a|b] [--roundtrip] [--save file]" << std::endl;
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI juce;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    Options opt;
    for (int i = 3; i < argc; ++i)
    {
        const juce::String a = juce::String::fromUTF8 (argv[i]);
        const bool hasNext = i + 1 < argc;
        if      (a == "--b"     && hasNext) opt.layerB = cwd.getChildFile (juce::String::fromUTF8 (argv[++i]));
        else if (a == "--split" && hasNext) opt.split  = juce::String (argv[++i]).getIntValue();
        else if (a == "--solo"  && hasNext) opt.solo   = juce::String (argv[++i]).equalsIgnoreCase ("b") ? 2 : 1;
        else if (a == "--save"  && hasNext) opt.saveTo = cwd.getChildFile (juce::String::fromUTF8 (argv[++i]));
        else if (a == "--roundtrip")        opt.roundtrip = true;
        else if (a == "--over-init")        opt.overInit  = true;
        else if (a == "--note"  && hasNext) opt.note   = juce::String (argv[++i]).getIntValue();
        else if (a == "--info")
        {
            juce::PluginHostType::jucePlugInClientCurrentWrapperType = juce::AudioProcessor::wrapperType_Undefined;
            SynthyProcessor p;
            int automatable = 0;
            for (auto* prm : p.getParameters()) automatable += prm->isAutomatable() ? 1 : 0;
            std::cout << "parameters " << p.getParameters().size() << " (automatable " << automatable
                      << "), per layer " << Modules::layeredParamIds().size() << std::endl;
            return 0;
        }
        else if (! a.startsWith ("--"))     opt.seconds = a.getDoubleValue();
        else { std::cerr << "JASS_Render: unknown option " << a << std::endl; return 2; }
    }
    int code = 0;
    {
        Job job (cwd.getChildFile (juce::String::fromUTF8 (argv[1])),
                 cwd.getChildFile (juce::String::fromUTF8 (argv[2])),
                 opt);
        if (job.exitCode == 0)
            juce::MessageManager::getInstance()->runDispatchLoop();
        code = job.exitCode;
    }
    return code;
}
