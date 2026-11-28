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
#include "../Audio/SeqMidiIO.h"   // review group 2: MIDI import / export per lane
#include "../Audio/ControllerProfiles.h"   // Story 22.1: controllerChecks
#include "../Modules/AllModules.h"         // Story 22.5: countable controls of real module descriptors

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
                setParam (*proc, Parameters::ID::layerFocus, 2.0f);   // A+B: both play
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
        s.patternChannelA = SynthyProcessor::kPatternChannel;
        s.patternChannelB = SynthyProcessor::kPatternChannelB;
        s.droneChannel = SynthyProcessor::kDroneChannel; s.auditionChannel = SynthyProcessor::kAuditionChannel;
        juce::MidiBuffer in, a, b;
        auto one = [&] (const juce::MidiMessage& m) { in.clear(); in.addEvent (m, 0); r.route (in, a, b, s); };
        one (juce::MidiMessage::noteOn (1, 59, 0.8f));  expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "split 60: key 59 -> B only");
        one (juce::MidiMessage::noteOn (1, 60, 0.8f));  expect (a.getNumEvents() == 1 && b.getNumEvents() == 0, "split 60: key 60 -> A only");
        s.splitNote = 40;   // the split moves while both keys are held
        one (juce::MidiMessage::noteOff (1, 59));       expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "split moved: 59's note-off still -> B");
        one (juce::MidiMessage::noteOff (1, 60));       expect (a.getNumEvents() == 1 && b.getNumEvents() == 0, "split moved: 60's note-off still -> A");
        s.splitNote = 60;
        one (juce::MidiMessage::noteOn (SynthyProcessor::kPatternChannelB, 84, 0.8f));
        expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "B's pattern note 84 (above the split) -> B only");
        one (juce::MidiMessage::noteOn (SynthyProcessor::kPatternChannel, 36, 0.8f));
        expect (a.getNumEvents() == 1 && b.getNumEvents() == 0, "A's pattern note 36 (below the split) -> A only");
        s.split = false;   // no split, both layers selected; then SPLIT while the key is held
        one (juce::MidiMessage::noteOn (1, 70, 0.8f));  expect (a.getNumEvents() == 1 && b.getNumEvents() == 1, "A+B, no split: key -> both");
        s.split = true;
        one (juce::MidiMessage::noteOff (1, 70));       expect (a.getNumEvents() == 1 && b.getNumEvents() == 1, "split switched on while held: note-off -> both");
        // 21.8: a layer whose pattern runs takes no live keys; the key still plays the other one.
        s.split = false; s.liveBlock = LayerRouter::kA;
        one (juce::MidiMessage::noteOn (1, 72, 0.8f));  expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "A+B, A's pattern running: key -> B only");
        s.liveBlock = LayerRouter::kNone;   // A's pattern stops while the key is held
        one (juce::MidiMessage::noteOff (1, 72));       expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "its note-off still -> B only");
        // Review 2026-11-24: B's step preview (channel 12) reaches B whatever is selected.
        s.playA = true; s.playB = false; s.split = false;
        s.auditionChannelB = SynthyProcessor::kAuditionChannelB;
        one (juce::MidiMessage::noteOn (SynthyProcessor::kAuditionChannelB, 60, 0.8f));
        expect (b.getNumEvents() == 1 && a.getNumEvents() == 0, "selection A: B's step preview (ch 12) -> B only");
        one (juce::MidiMessage::noteOff (SynthyProcessor::kAuditionChannelB, 60));
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
        setParam (*built.p, ID::layerFocus, 2.0f);              // saved with A+B lit
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
            setParam (*r.p, b + ID::filterCutoff, 777.0f);
            setParam (*r.p, ID::layerFocus, 0.0f);
            PresetIO::loadPreset (r.p->getAPVTS(), tmpB, PresetIO::LoadScope::KeepLayerB);
            expect (std::abs (rawOf (*r.p, b + ID::filterCutoff) - 1234.0f) < 0.5f,
                    "A-only load of a file WITH LayerB loads it whole (2026-11-23)");
            expect (juce::roundToInt (rawOf (*r.p, ID::layerFocus)) == 2 && juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 2,
                    "a two-layer file brings its A / B selection and its split");
            PresetIO::loadPreset (r.p->getAPVTS(), tmpB, PresetIO::LoadScope::Whole);
            expect (std::abs (rawOf (*r.p, b + ID::filterCutoff) - 1234.0f) < 0.5f, "A+B load of a file with LayerB replaces B");
        }
        {
            // LOAD with B selected: the file's sound lands on B's ids; A and the instrument stay.
            Rig ref;   // what the file's layer A looks like, loaded the ordinary way
            PresetIO::loadPreset (ref.p->getAPVTS(), tmpOld, PresetIO::LoadScope::KeepLayerB);
            Rig r;
            setParam (*r.p, ID::filterCutoff, 777.0f);
            setParam (*r.p, ID::layerMode, 2.0f);
            PresetIO::loadPreset (r.p->getAPVTS(), tmpOld, PresetIO::LoadScope::IntoLayerB);
            int bad = 0;
            for (const auto& id : Modules::layeredParamIds())
                if (id != ID::samplerSet)   // a set index is session-local; resolved by name, maybe still loading
                    if (std::abs (rawOf (*r.p, b + id) - rawOf (*ref.p, id)) > 1.0e-4f)
                        if (bad++ < 5) std::cerr << "        B differs from the file: " << id << std::endl;
            expect (bad == 0, "load into B: the file's sound is on layer B");
            expect (std::abs (rawOf (*r.p, ID::filterCutoff) - 777.0f) < 0.5f && juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 2,
                    "load into B: layer A and the mode untouched");
        }
        tmpOld.deleteFile();
        tmpB.deleteFile();

        // ---- Story 21.8: the figure travels with its layer ---------------------------------------
        std::cout << "STEP SEQ per layer in presets (FormatVersion 12)" << std::endl;
        auto figureIs = [] (const StepPattern& f, int pitch0) { return f.on (0) && f.pitch (0) == pitch0 && f.lastUsed() == 0; };
        {
            Rig src;   // B's own figure + B's STEP SEQ on, A without one
            auto& fb = src.p->getPatterns().steps[1];
            fb.setOn (0, true); fb.setPitch (0, 7);
            setParam (*src.p, b + ID::seqOn, 1.0f);
            const auto v = PresetIO::toVar (src.p->getAPVTS(), "fig");
            expect (v["LayerB"]["StepSeq"]["Steps"].isArray(), "B's figure is written inside LayerB.StepSeq");
            expect (! v["Instrument"]["StepSeq"].isObject(), "no StepSeq left in Instrument");
            Rig dst;
            PresetIO::applyVar (dst.p->getAPVTS(), juce::JSON::parse (juce::JSON::toString (v, false, 12)));
            expect (figureIs (dst.p->getPatterns().steps[1], 7) && dst.p->getPatterns().steps[0].lastUsed() < 0,
                    "save -> load: B's figure back in B, A's stays empty");
            expect (rawOf (*dst.p, b + ID::seqOn) > 0.5f, "...and B's STEP SEQ switch with it");
        }
        // A v11 file: STEP SEQ (with a TARGET) and ARP lived in "Instrument". Built from a v12 file by
        // moving the objects back, as a v11 writer had them.
        auto asV11 = [] (juce::var v, const juce::String& target)
        {
            v.getDynamicObject()->setProperty ("FormatVersion", 11);
            auto* inst = v["Instrument"].getDynamicObject();
            auto* la   = v["LayerA"].getDynamicObject();
            auto seq = la->getProperty ("StepSeq");
            seq.getDynamicObject()->setProperty ("LayerTarget", target);
            inst->setProperty ("StepSeq", seq);
            inst->setProperty ("Arp", la->getProperty ("Arp"));
            la->removeProperty ("StepSeq");
            la->removeProperty ("Arp");
            return v;
        };
        {
            Rig src;   // A's figure, A's STEP SEQ on
            auto& fa = src.p->getPatterns().steps[0];
            fa.setOn (0, true); fa.setPitch (0, 5);
            setParam (*src.p, ID::seqOn, 1.0f);
            setParam (*src.p, ID::seqLength, 12.0f);
            const auto text = juce::JSON::toString (PresetIO::toVar (src.p->getAPVTS(), "v11"), false, 12);
            {
                Rig dst;
                PresetIO::applyVar (dst.p->getAPVTS(), asV11 (juce::JSON::parse (text), "B"));
                const auto& pt = dst.p->getPatterns();
                expect (figureIs (pt.steps[1], 5) && pt.steps[0].lastUsed() < 0, "v11 TARGET B: the figure lands in B, A stays empty");
                expect (rawOf (*dst.p, b + ID::seqOn) > 0.5f && rawOf (*dst.p, ID::seqOn) < 0.5f
                        && std::abs (rawOf (*dst.p, b + ID::seqLength) - 12.0f) < 0.5f, "v11 TARGET B: B's STEP SEQ on with the file's LEN, A's off");
            }
            {
                Rig dst;
                PresetIO::applyVar (dst.p->getAPVTS(), asV11 (juce::JSON::parse (text), "A+B"));
                const auto& pt = dst.p->getPatterns();
                expect (figureIs (pt.steps[0], 5) && figureIs (pt.steps[1], 5), "v11 TARGET A+B: the figure in both layers");
            }
            {
                Rig dst;
                PresetIO::applyVar (dst.p->getAPVTS(), asV11 (juce::JSON::parse (text), "Auto"));
                const auto& pt = dst.p->getPatterns();
                expect (figureIs (pt.steps[0], 5) && pt.steps[1].lastUsed() < 0 && rawOf (*dst.p, ID::seqOn) > 0.5f,
                        "v11 TARGET Auto: the figure in A, as before");
            }
        }
        {
            // A single-layer file keeps the player's B — its figure included; a two-layer file
            // brings its own B figure whatever is lit (2026-11-23).
            const auto tmpF = juce::File::createTempFile (".jass");   // with a LayerB of its own
            const auto tmp1 = juce::File::createTempFile (".jass");   // layer A only
            {
                Rig one;   // untouched B: no LayerB in the file
                one.p->getPatterns().steps[0].setOn (0, true);
                tmp1.replaceWithText (juce::JSON::toString (PresetIO::toVar (one.p->getAPVTS(), "one"), false, 12));
                Rig src;   // a different B figure and sound
                auto& fb = src.p->getPatterns().steps[1];
                fb.setOn (0, true); fb.setPitch (0, 3);
                setParam (*src.p, b + ID::filterCutoff, 1234.0f);
                tmpF.replaceWithText (juce::JSON::toString (PresetIO::toVar (src.p->getAPVTS(), "f"), false, 12));
            }
            expect (! juce::JSON::parse (tmp1)["LayerB"].isObject(), "an untouched B stays out of the file");
            Rig r;
            auto& fb = r.p->getPatterns().steps[1];
            fb.setOn (0, true); fb.setPitch (0, 9);
            PresetIO::loadPreset (r.p->getAPVTS(), tmp1, PresetIO::LoadScope::KeepLayerB);
            expect (figureIs (r.p->getPatterns().steps[1], 9), "a single-layer load keeps B's figure");
            PresetIO::loadPreset (r.p->getAPVTS(), tmpF, PresetIO::LoadScope::KeepLayerB);
            expect (figureIs (r.p->getPatterns().steps[1], 3), "a two-layer load brings its B figure, A alone lit");
            fb.setPitch (0, 9);
            PresetIO::loadPreset (r.p->getAPVTS(), tmpF, PresetIO::LoadScope::Whole);
            expect (figureIs (r.p->getPatterns().steps[1], 3), "A+B load brings the file's B figure");
            tmpF.deleteFile();
            tmp1.deleteFile();
        }

        // ---- code review 2026-11-24, group 2 ----------------------------------------------------
        std::cout << "Latch root per lane through presets" << std::endl;
        {
            Rig src;
            src.p->getPatterns().steps[0].setOn (0, true);
            src.p->getPatterns().steps[1].setOn (0, true);
            PresetIO::hooks (src.p->getAPVTS()).applySeqLatchRoot (0, 60);
            PresetIO::hooks (src.p->getAPVTS()).applySeqLatchRoot (1, 50);
            const auto v = PresetIO::toVar (src.p->getAPVTS(), "latch");
            Rig dst;
            PresetIO::applyVar (dst.p->getAPVTS(), juce::JSON::parse (juce::JSON::toString (v, false, 12)));
            expect (dst.p->getSeqLatchRoot (0) == 60 && dst.p->getSeqLatchRoot (1) == 50, "save -> load: A latched on 60, B on 50");
        }
        std::cout << "A B figure or a B latch alone makes the file two-layer" << std::endl;
        {
            Rig fig;   // B's figure only: no B parameter changed, B's STEP SEQ off
            fig.p->getPatterns().steps[1].setOn (2, true);
            expect (PresetIO::toVar (fig.p->getAPVTS(), "f")["LayerB"].isObject(), "a B figure alone is written (LayerB)");
            Rig lat;   // B latched only, empty figure
            PresetIO::hooks (lat.p->getAPVTS()).applySeqLatchRoot (1, 50);
            expect (PresetIO::toVar (lat.p->getAPVTS(), "l")["LayerB"].isObject(), "a B latch alone is written (LayerB)");
        }
        std::cout << "Load into B carries the figure into B and leaves A's" << std::endl;
        {
            const auto tmpS = juce::File::createTempFile (".jass");
            {
                Rig src;   // a single-layer file whose layer A has a figure
                auto& fa = src.p->getPatterns().steps[0];
                fa.setOn (0, true); fa.setPitch (0, 4);
                setParam (*src.p, ID::seqOn, 1.0f);
                tmpS.replaceWithText (juce::JSON::toString (PresetIO::toVar (src.p->getAPVTS(), "s"), false, 12));
            }
            Rig r;
            auto& fa = r.p->getPatterns().steps[0];
            fa.setOn (0, true); fa.setPitch (0, 9);   // the player's A figure
            PresetIO::loadPreset (r.p->getAPVTS(), tmpS, PresetIO::LoadScope::IntoLayerB);
            expect (figureIs (r.p->getPatterns().steps[1], 4), "load into B: the file's figure is B's");
            expect (figureIs (r.p->getPatterns().steps[0], 9), "load into B: A's figure untouched");
            expect (rawOf (*r.p, b + ID::seqOn) > 0.5f, "load into B: B's STEP SEQ on, as in the file");
            tmpS.deleteFile();
        }
        std::cout << "A stored Dual and a develop B ON file read as no split" << std::endl;
        {
            Rig base;
            const auto text = juce::JSON::toString (PresetIO::toVar (base.p->getAPVTS(), "d"), false, 12);
            auto withLayers = [&text] (int version, juce::DynamicObject* lay)
            {
                auto v = juce::JSON::parse (text);
                v.getDynamicObject()->setProperty ("FormatVersion", version);
                v["Instrument"].getDynamicObject()->setProperty ("Layers", juce::var (lay));
                return v;
            };
            {
                auto* lay = new juce::DynamicObject(); lay->setProperty ("Mode", "Dual");
                Rig r; r.set (ID::layerMode, 2.0f);
                PresetIO::applyVar (r.p->getAPVTS(), withLayers (12, lay));
                expect (juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 0, "Mode Dual reads as no split");
            }
            {
                auto* lay = new juce::DynamicObject(); lay->setProperty ("BOn", true); lay->setProperty ("Mode", "Layer");
                Rig r; r.set (ID::layerMode, 2.0f);
                PresetIO::applyVar (r.p->getAPVTS(), withLayers (11, lay));
                expect (juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 0, "develop B ON + Layer reads as no split");
            }
            {
                auto* lay = new juce::DynamicObject(); lay->setProperty ("BOn", true); lay->setProperty ("Mode", "Split");
                Rig r;
                PresetIO::applyVar (r.p->getAPVTS(), withLayers (11, lay));
                expect (juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 2, "develop B ON + Split reads as Split");
            }
            {
                auto* lay = new juce::DynamicObject(); lay->setProperty ("BOn", false); lay->setProperty ("Mode", "Split");
                Rig r;
                PresetIO::applyVar (r.p->getAPVTS(), withLayers (11, lay));
                expect (juce::roundToInt (rawOf (*r.p, ID::layerMode)) == 0, "develop B off + Split reads as no split (only the BOn pass knows)");
            }
        }
        std::cout << "MIDI export / import on B's STEP SEQ leaves A's alone" << std::endl;
        {
            const auto mid = juce::File::createTempFile (".mid");
            Rig src;
            auto& fb = src.p->getPatterns().steps[1];
            const int pitches[4] = { 0, 7, 12, 5 };
            for (int i = 0; i < 16; ++i) { fb.setOn (i, (i % 4) == 0); fb.setPitch (i, pitches[i / 4]); }
            setParam (*src.p, b + ID::seqLength, 16.0f);
            setParam (*src.p, b + ID::seqOn, 1.0f);
            PresetIO::hooks (src.p->getAPVTS()).applySeqLatchRoot (1, 48);
            expect (SeqMidiIO::exportFigure (src.p->getAPVTS(), mid, 1), "B's figure exported");
            Rig dst;
            setParam (*dst.p, ID::seqLength, 7.0f);   // A's own settings, to be left alone
            const auto res = SeqMidiIO::importFigure (dst.p->getAPVTS(), mid, 1);
            expect (res.error == SeqMidiIO::Error::none, "B's figure imported into B");
            const auto& got = dst.p->getPatterns().steps[1];
            // The import re-grids the file (its own step division and gate reading), so compare the
            // melody: the pitches of the sounding steps in order, repeats folded, relative to the first.
            std::vector<int> mel;
            for (int i = 0; i < StepPattern::kMaxSteps; ++i)
                if (got.on (i) && (mel.empty() || mel.back() != got.pitch (i))) mel.push_back (got.pitch (i));
            const bool shape = mel.size() == 4 && mel[1] - mel[0] == 7 && mel[2] - mel[0] == 12 && mel[3] - mel[0] == 5;
            expect (shape, "round trip: B's figure keeps its intervals");
            expect (rawOf (*dst.p, b + ID::seqOn) > 0.5f, "import into B switches B's STEP SEQ on");
            expect (rawOf (*dst.p, ID::seqOn) < 0.5f && std::abs (rawOf (*dst.p, ID::seqLength) - 7.0f) < 0.5f
                    && dst.p->getPatterns().steps[0].lastUsed() < 0, "A's STEP SEQ, LEN and figure untouched");
            mid.deleteFile();
        }
    }

    void processorChecks()
    {
        using namespace Parameters;
        const juce::String bp (Modules::kLayerBPrefix);
        std::cout << "Split + B's ARP: a live tap above the split" << std::endl;
        {
            Rig r;
            r.set (ID::layerMode, 2.0f); r.set (ID::splitNote, 60.0f); setParam (*r.p, bp + ID::arpOn, 1.0f);
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
            auto& st = r.p->getPatterns().steps[0];
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

        // ---- Story 21.8: a STEP SEQ per layer --------------------------------------------------
        // A one-step figure with a long step (2 s, gate 1): a latched lane keeps exactly one key down
        // in ITS layer, which is what these checks count.
        auto longFigure = [&] (Rig& r, int lane)
        {
            const juce::String pre = lane == 0 ? juce::String() : bp;
            auto& st = r.p->getPatterns().steps[(size_t) lane];
            st.setOn (0, true); st.setPitch (0, 0);
            setParam (*r.p, pre + ID::seqLength, 1.0f); setParam (*r.p, pre + ID::seqSync, 0.0f);
            setParam (*r.p, pre + ID::seqRate, 0.5f);   setParam (*r.p, pre + ID::seqGate, 1.0f);
            setParam (*r.p, pre + ID::seqOn, 1.0f);
        };
        std::cout << "Two STEP SEQs at once (Split: left hand B's, right hand A's)" << std::endl;
        {
            Rig r;
            r.set (ID::layerMode, 2.0f); r.set (ID::splitNote, 60.0f);
            longFigure (r, 0); longFigure (r, 1);
            r.run (0.1);
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 72, 0.8f) }, { 0, juce::MidiMessage::noteOn (1, 48, 0.8f) } });
            r.block ({ { 0, juce::MidiMessage::noteOff (1, 72) }, { 0, juce::MidiMessage::noteOff (1, 48) } });
            r.run (0.3);
            expect (r.p->isSeqLatched (0) && r.p->isSeqLatched (1), "each lane latched from its own hand");
            expect (r.p->getSeqLatchRoot (0) == 72 && r.p->getSeqLatchRoot (1) == 48, "lane A's root is the right hand's key, lane B's the left hand's");
            expect (r.p->layerKeysDown (false) == 1 && r.p->layerKeysDown (true) == 1, "both figures sound, one note in each layer");
            setParam (*r.p, bp + ID::seqOn, 0.0f);   // B's STEP SEQ off while A's runs on
            r.run (0.6);
            expect (r.p->layerKeysDown (true) == 0, "B's STEP SEQ switched off: B's note ends (no hang)");
            expect (r.p->layerKeysDown (false) == 1, "...and A's figure keeps running");
        }
        std::cout << "A lane plays only its own layer" << std::endl;
        {
            Rig r;   // selection A, B's figure latched without a key (as a loaded preset starts it)
            longFigure (r, 1);
            PresetIO::hooks (r.p->getAPVTS()).applySeqLatchRoot (1, 48);
            r.run (0.4);
            expect (r.p->layerKeysDown (true) == 1 && r.p->layerKeysDown (false) == 0, "B's latched figure sounds in B, nothing in A");
        }
        std::cout << "A+B: A's ARP takes the key, B plays it live; selection change while held" << std::endl;
        {
            Rig r;
            r.set (ID::layerFocus, 2.0f); r.set (ID::arpOn, 1.0f);
            r.run (0.1);
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 60, 0.8f) } });
            r.run (0.3);
            expect (r.p->layerKeysDown (true) == 1, "B plays the held key live");
            expect (r.p->layerKeysDown (false) >= 1, "A plays the arpeggio of it");
            r.set (ID::layerFocus, 1.0f);   // to B only while the key is held
            r.run (0.1);
            r.block ({ { 0, juce::MidiMessage::noteOff (1, 60) } });
            r.run (1.5);
            expect (r.p->layerKeysDown (true) == 0 && r.p->layerKeysDown (false) == 0, "key released after the switch: nothing left down in A or B");
        }
        std::cout << "ARP / STEP SEQ exclusion is per layer" << std::endl;
        {
            Rig r;
            r.set (ID::arpOn, 1.0f);
            setParam (*r.p, bp + ID::arpOn, 1.0f);
            setParam (*r.p, bp + ID::seqOn, 1.0f);   // B's STEP SEQ on: B's ARP goes off ...
            auto raw = [&] (const juce::String& id) { return r.p->getAPVTS().getRawParameterValue (id)->load(); };
            expect (raw (bp + ID::arpOn) < 0.5f, "B's STEP SEQ on switches B's ARP off");
            expect (raw (ID::arpOn) > 0.5f, "...and leaves A's ARP on");
        }

        // ---- code review 2026-11-24, group 1 --------------------------------------------------
        std::cout << "B's step preview sounds in B with A selected" << std::endl;
        {
            Rig r;   // selection A, nothing else playing
            r.run (0.1);
            r.block ({ { 0, juce::MidiMessage::noteOn (SynthyProcessor::kAuditionChannelB, 60, 0.8f) } });
            r.run (0.1);
            expect (r.p->layerKeysDown (true) == 1 && r.p->layerKeysDown (false) == 0, "preview on ch 12: one key down in B, none in A");
            r.block ({ { 0, juce::MidiMessage::noteOff (SynthyProcessor::kAuditionChannelB, 60) } });
        }
        std::cout << "Host panic on channel 1 stops B's latched figure note too" << std::endl;
        {
            Rig r;
            longFigure (r, 1);
            PresetIO::hooks (r.p->getAPVTS()).applySeqLatchRoot (1, 48);
            r.run (0.3);
            expect (r.p->layerKeysDown (true) >= 1, "B's figure note sounding (latched) before the panic");
            r.block ({ { 0, juce::MidiMessage::allNotesOff (1) } });
            r.run (0.6);
            expect (r.p->layerKeysDown (true) == 0, "All-Notes-Off on channel 1: B's pattern note has ended too");
        }
        std::cout << "Editing B's figure marks the preset modified" << std::endl;
        {
            Rig r;
            r.p->markPresetClean();
            expect (! r.p->isPresetModified(), "clean after markPresetClean");
            r.p->getPatterns().steps[1].setOn (3, true);
            expect (r.p->isPresetModified(), "a step switched in B's figure: modified");
        }
        std::cout << "DAW state carries B's figure and latch" << std::endl;
        {
            Rig src;
            auto& fb = src.p->getPatterns().steps[1];
            fb.setOn (0, true); fb.setPitch (0, 5);
            setParam (*src.p, bp + ID::seqOn, 1.0f);
            PresetIO::hooks (src.p->getAPVTS()).applySeqLatchRoot (1, 50);
            juce::MemoryBlock mb;
            src.p->getStateInformation (mb);
            Rig dst;
            dst.p->setStateInformation (mb.getData(), (int) mb.getSize());
            const auto& pt = dst.p->getPatterns();
            expect (pt.steps[1].on (0) && pt.steps[1].pitch (0) == 5, "B's figure back in B");
            expect (pt.steps[0].lastUsed() < 0, "A's figure stays empty");
            expect (dst.p->getSeqLatchRoot (1) == 50, "B's latch back (SeqLatchRootB)");
        }

        // ---- code review 2026-11-24, group 2 ----------------------------------------------------
        std::cout << "A single-layer load keeps B's running figure running" << std::endl;
        {
            const auto tmp1 = juce::File::createTempFile (".jass");
            {
                Rig one;   // no LayerB in the file
                tmp1.replaceWithText (juce::JSON::toString (PresetIO::toVar (one.p->getAPVTS(), "one"), false, 12));
            }
            Rig r;   // B's 8-step figure at 4 steps/s, latched, selection A
            auto& st = r.p->getPatterns().steps[1];
            for (int i = 0; i < 8; ++i) { st.setOn (i, true); st.setPitch (i, i); }
            setParam (*r.p, bp + ID::seqLength, 8.0f); setParam (*r.p, bp + ID::seqSync, 0.0f);
            setParam (*r.p, bp + ID::seqRate, 4.0f);   setParam (*r.p, bp + ID::seqGate, 1.0f);
            setParam (*r.p, bp + ID::seqOn, 1.0f);
            PresetIO::hooks (r.p->getAPVTS()).applySeqLatchRoot (1, 48);
            r.run (0.6);
            const int before = r.p->getSeqStep (1);
            PresetIO::loadPreset (r.p->getAPVTS(), tmp1, PresetIO::LoadScope::KeepLayerB);
            r.block();
            expect (r.p->getSeqLatchRoot (1) == 48, "B's latch kept");
            expect (before > 0 && r.p->getSeqStep (1) == before, "B's figure runs on (no restart at step 1)");
            expect (r.p->layerKeysDown (true) == 1, "B's note still sounding through the load");
            tmp1.deleteFile();
        }
        std::cout << "A+B linked editing mirrors a knob, not a module switch" << std::endl;
        {
            Rig r;
            r.set (ID::layerFocus, 2.0f);
            auto raw = [&] (const juce::String& id) { return r.p->getAPVTS().getRawParameterValue (id)->load(); };
            const float bCut = raw (bp + ID::filterCutoff);
            setParam (*r.p, ID::filterCutoff, raw (ID::filterCutoff) * 2.0f);
            expect (std::abs (raw (bp + ID::filterCutoff) - bCut) > 1.0f, "A's CUTOFF moved: B's CUTOFF follows");
            const float bOn = raw (bp + ID::filterOn);
            setParam (*r.p, ID::filterOn, bOn);                         // A matches B first ...
            setParam (*r.p, ID::filterOn, bOn > 0.5f ? 0.0f : 1.0f);   // ... then flips: a mirror would carry it to B
            expect (std::abs (raw (bp + ID::filterOn) - bOn) < 0.5f, "A's FILTER switched: B's stays as it was");
        }
        std::cout << "Step entry per lane: B's armed lane takes no root, A still latches" << std::endl;
        {
            Rig r;
            r.set (ID::layerMode, 2.0f); r.set (ID::splitNote, 60.0f);
            longFigure (r, 0); longFigure (r, 1);
            r.p->setSeqRecordArmed (1, true);   // writing B's figure by playing (review group 3)
            r.run (0.1);
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 48, 0.8f) }, { 0, juce::MidiMessage::noteOn (1, 72, 0.8f) } });
            r.block ({ { 0, juce::MidiMessage::noteOff (1, 48) }, { 0, juce::MidiMessage::noteOff (1, 72) } });
            r.run (0.2);
            expect (! r.p->isSeqLatched (1), "an armed lane B takes the left hand's key as an entry, not as a root");
            expect (r.p->isSeqLatched (0) && r.p->getSeqLatchRoot (0) == 72, "lane A still latches from the right hand");
            r.p->setSeqRecordArmed (1, false);
        }
        std::cout << "A per-key One-Shot LFO's ring restarts on a key (A and B)" << std::endl;
        for (const bool layerB : { false, true })
        {
            Rig r;
            const juce::String pre = layerB ? bp : juce::String();
            r.set (ID::layerFocus, layerB ? 1.0f : 0.0f);   // the key reaches the layer under test
            setParam (*r.p, pre + ID::lfoOn (1), 1.0f);
            setParam (*r.p, pre + ID::lfoWave (1), 5.0f);    // One-Shot
            setParam (*r.p, pre + ID::lfoRate (1), 3.5f);
            setParam (*r.p, pre + ID::lfoDepth (1), 1.0f);
            setParam (*r.p, pre + ID::lfoGlobal (1), 0.0f);  // per voice
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 60, 0.8f) } });
            r.run (1.0);                                     // the sweep is over: the ring sits at rest
            const float rest = r.p->getLfoDisplayValueOf (layerB, 0);
            r.block ({ { 0, juce::MidiMessage::noteOff (1, 60) } });
            r.block ({ { 0, juce::MidiMessage::noteOn (1, 62, 0.8f) } });
            r.run (0.02);
            expect (std::abs (r.p->getLfoDisplayValueOf (layerB, 0) - rest) > 0.2f,
                    layerB ? "B: the One-Shot ring sweeps again on the next key" : "A: the One-Shot ring sweeps again on the next key");
        }
        std::cout << "ARP SYNC: 1/16 at 120 BPM is 8 steps a second; Free follows RATE (A and B)" << std::endl;
        for (const bool layerB : { false, true })
        {
            const juce::String pre = layerB ? bp : juce::String();
            auto stepsPerSecond = [&] (int syncIndex, float rate)
            {
                Rig r;
                r.set (ID::layerFocus, layerB ? 1.0f : 0.0f);
                r.set (ID::syncTempo, 120.0f);
                setParam (*r.p, pre + ID::arpOctaves, 1.0f);
                setParam (*r.p, pre + ID::arpGate, 0.5f);
                setParam (*r.p, pre + ID::arpRate, rate);
                setParam (*r.p, pre + ID::arpSync, (float) syncIndex);
                setParam (*r.p, pre + ID::arpOn, 1.0f);
                r.block ({ { 0, juce::MidiMessage::noteOn (1, 60, 0.8f) } });   // held for the whole count
                int starts = 0, was = 0;
                for (int i = 0; i < (int) (2.0 * kSampleRate / kBlock); ++i)
                {
                    r.block();
                    const int now = r.p->layerKeysDown (layerB);
                    if (now > 0 && was == 0) ++starts;
                    was = now;
                }
                return starts / 2.0;
            };
            const int sixteenth = SyncDivision::kNames.indexOf ("1/16");
            expect (sixteenth > 0, "SyncDivision has a 1/16");
            const double synced = stepsPerSecond (sixteenth, 3.0f);   // RATE 3 must not matter
            const double free   = stepsPerSecond (0, 3.0f);
            expect (std::abs (synced - 8.0) <= 1.0, layerB ? "B: ARP SYNC 1/16 at 120 BPM ~ 8 steps/s" : "A: ARP SYNC 1/16 at 120 BPM ~ 8 steps/s");
            expect (std::abs (free - 3.0) <= 1.0,   layerB ? "B: ARP Free follows RATE 3"               : "A: ARP Free follows RATE 3");
        }
    }
}

namespace
{
    // Story 22.1: controller profiles — parse (shipped files, unknown id, broken JSON), matching and
    // fallback, hand pick, focus A / B / A+B and pinned entries, non-layered ids, reserved channels,
    // and the host path (a DAW: CCs move nothing).
    void controllerChecks()
    {
        std::cout << "Controller profiles" << std::endl;
        using CP = ControllerProfiles;
        Rig r;
        auto exists = [&] (const juce::String& id) { return r.p->getAPVTS().getParameter (id) != nullptr; };

        // The shipped files parse against the real parameter layout, every entry known.
        std::vector<CP::Profile> shipped;
        for (int i = 0; i < ControllerProfileFiles::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = ControllerProfileFiles::getNamedResource (ControllerProfileFiles::namedResourceList[i], size);
            CP::Profile p;
            if (data != nullptr && CP::parseText (juce::String::fromUTF8 (data, size), "?", exists, p))
                shipped.push_back (p);
        }
        expect (shipped.size() == 2, "both factory profiles parse");
        for (const auto& p : shipped)
        {
            if (p.name == "MiniLab 3") expect (p.controls.size() == 30, "MiniLab 3: all 30 controls known (ARTURIA + DAW program, each with Shift, Shift + encoder and the encoder click)");
            if (p.name == CP::kGenericName) expect (p.controls.size() == 6, "Generic (GM): all 6 controls known");
        }

        CP::Profile test;
        const bool parsed = CP::parseText (R"({ "name": "Test", "match": "Test Box", "controls": [
              { "name": "ok",     "cc": 74, "channel": 0, "param": "filterCutoff", "takeover": "jump" },
              { "name": "nope",   "cc": 75, "channel": 0, "param": "nope" },
              { "name": "badcc",  "cc": 300, "param": "filterReso" },
              { "name": "nocc",   "param": "filterReso" },
              { "name": "hugecc", "cc": 1e300, "param": "filterReso" },
              { "name": "strch",  "cc": 77, "channel": "2", "param": "filterReso" },
              { "name": "resch",  "cc": 78, "channel": 15, "param": "filterReso" },
              { "name": "badpin", "cc": 76, "param": "filterReso", "layer": "C" },
              { "name": "pinB",   "cc": 20, "channel": 2, "param": "filterCutoff", "layer": "B", "takeover": "jump" },
              { "name": "pinA",   "cc": 21, "param": "filterCutoff", "layer": "a", "takeover": "jump" },
              { "name": "tempo",  "cc": 22, "param": "syncTempo", "takeover": "jump" },
              42 ] })", "fallback", exists, test);
        expect (parsed && test.name == "Test", "profile parses");
        expect (test.controls.size() == 4, "unknown id and malformed entries skipped, the rest kept");
        CP::Profile broken;
        expect (! CP::parseText ("{ \"name\": \"Broken\", \"controls\": [ ", "Broken", exists, broken), "broken JSON: not offered");
        expect (! CP::parseText ("{ \"name\": \"NoControls\" }", "NoControls", exists, broken), "no controls array: not offered");

        auto ids = [] (const std::vector<CP::Target>& t) { juce::StringArray a; for (const auto& x : t) a.add (x.paramId + (x.pinned ? "!" : "")); return a.joinIntoString (","); };
        expect (ids (CP::resolve (test, 1, 74, 0)) == "filterCutoff",  "focus A: CC74 -> filterCutoff");
        expect (ids (CP::resolve (test, 1, 74, 1)) == "bfilterCutoff", "focus B: CC74 -> bfilterCutoff");
        expect (ids (CP::resolve (test, 1, 74, 2)) == "filterCutoff",  "focus A+B: CC74 -> A (linked editing moves B)");
        expect (ids (CP::resolve (test, 2, 20, 0)) == "bfilterCutoff!", "pinned B moves B in focus A");
        expect (ids (CP::resolve (test, 1, 20, 0)).isEmpty(),            "a channel-bound entry ignores other channels");
        expect (ids (CP::resolve (test, 1, 21, 2)) == "filterCutoff!",  "pinned A in A+B: A only (no mirror)");
        expect (ids (CP::resolve (test, 1, 22, 1)) == "syncTempo",      "non-layered id set by its own id in focus B");
        expect (ids (CP::resolve (test, 1, 5, 0)).isEmpty(),             "unmapped CC: nothing");
        expect (ids (CP::resolve (test, 15, 74, 0)).isEmpty() && ids (CP::resolve (test, 12, 74, 0)).isEmpty(),
                "reserved channels 12..16 ignored");
        expect (ids (CP::resolve (test, 11, 74, 0)) == "filterCutoff",  "channel 11 still read");

        std::vector<CP::Profile> set = shipped;
        set.push_back (test);
        std::sort (set.begin(), set.end(), [] (const CP::Profile& a, const CP::Profile& b) { return a.name.compareNatural (b.name) < 0; });
        auto pickName = [&] (juce::StringArray in, juce::String hand) { const int i = CP::pick (set, in, hand); return i < 0 ? juce::String ("-") : set[(size_t) i].name; };
        expect (pickName ({ "Minilab3 MIDI" }, {}) == "MiniLab 3",               "port Minilab3 MIDI -> MiniLab 3");
        expect (pickName ({ "UM-ONE" }, {}) == CP::kGenericName,                "no match (UM-ONE) -> Generic (GM)");
        expect (pickName ({}, {}) == CP::kGenericName,                          "no input -> Generic (GM)");
        expect (pickName ({ "UM-ONE" }, "MiniLab 3") == "MiniLab 3",            "hand pick wins over matching");
        expect (pickName ({ "Minilab3 MIDI" }, "Gone") == "MiniLab 3",          "a vanished hand pick falls back to Auto");
        expect (pickName ({ "Minilab3 MIDI" }, "Auto") == "MiniLab 3",          "'Auto' is automatic");
        expect (pickName ({ "UM-ONE", "TESTBOX MIDI" }, {}) == "Test",         "match ignores case and spaces (Test Box -> TESTBOX MIDI)");
        std::vector<CP::Profile> none;
        expect (CP::pick (none, { "UM-ONE" }, {}) == -1,                        "no profiles: none active");

        // In a DAW (the Rig is the host path) a CC reaches no parameter.
        auto* cutoff = r.p->getAPVTS().getParameter (Parameters::ID::filterCutoff);
        auto* cutoffB = r.p->getAPVTS().getParameter (juce::String (Modules::kLayerBPrefix) + Parameters::ID::filterCutoff);
        const float before = cutoff->getValue();
        r.block ({ { 0, juce::MidiMessage::controllerEvent (1, 74, 127) } });
        expect (cutoff->getValue() == before && ! r.p->anyControllerCapturedForTest(), "hosted: CC74 captured nothing, moves nothing");

        // The CC path itself (capture -> dirty bits -> poll -> parameter), through the test seam.
        auto setup = [&] (float a01, float b01, int focus)
        {
            r.set (Parameters::ID::layerFocus, 0.0f);   // focus A while placing: no mirroring
            cutoff->setValueNotifyingHost (a01);
            cutoffB->setValueNotifyingHost (b01);
            r.set (Parameters::ID::layerFocus, (float) focus);
        };
        auto cc = [&] (int ch, int num, int val)
        {
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::controllerEvent (ch, num, val), 0);
            r.p->runControllersForTest ({ test }, m);
        };
        auto near = [] (float x, float y) { return std::abs (x - y) < 0.005f; };
        auto tight = [] (float x, float y) { return std::abs (x - y) < 0.0005f; };
        const float u = 1.0f / 127.0f;   // one CC unit of a control's travel
        // 22.8: these checks are about the path and the layer rules, so the test profile opts out
        // of soft takeover ("takeover": "jump") - the position is the value, as in 22.1.
        setup (0.6f, 0.2f, 2);
        {
            const float a0 = cutoff->getValue(), b0 = cutoffB->getValue();
            cc (1, 74, 127);
            expect (cutoff->getValue() == 1.0f, "A+B: CC74=127 sets A's cutoff to the top");
            expect (near (cutoffB->getValue(), b0 + (1.0f - a0)), "A+B: B follows by the same knob distance");
            expect (! r.p->anyControllerCapturedForTest(), "poll drains every dirty bit");
        }
        setup (0.6f, 0.2f, 2);
        {
            const float b0 = cutoffB->getValue();
            cc (1, 21, 127);
            expect (cutoff->getValue() == 1.0f && cutoffB->getValue() == b0, "A+B: pinned A moves A only");
        }
        setup (0.3f, 0.2f, 0);
        cc (11, 74, 127);
        expect (cutoff->getValue() == 1.0f, "channel 11 CC lands and applies");
        setup (0.3f, 0.2f, 0);
        cc (1, 74, 64);
        expect (near (cutoff->getValue(), 64.0f / 127.0f), "CC 64 -> 64/127 of the travel");
        setup (0.3f, 0.2f, 0);
        cc (15, 74, 127);
        expect (near (cutoff->getValue(), 0.3f), "reserved channel 15: not captured");
        r.set (Parameters::ID::layerFocus, 0.0f);

        //------------------------------------------------------------------------------------------
        // Story 22.5, step a: special targets, the value math, Shift, fine tempo, the faders on the
        // globals, and the selection behind its test hook (no editor pushes a frame list yet).
        std::cout << "Controller profiles: knobs follow the module (22.5, processor side)" << std::endl;

        expect (CP::stepIndex (0.0f, 6) == 0 && CP::stepIndex (1.0f, 6) == 5, "selector: 0 / 127 pick the first / last of 6");
        expect (CP::stepIndex (100.0f / 127.0f, 4) == 3, "combo with 4 items, knob at 100: item 3");
        expect (CP::stepIndex (0.5f, 2) == 1 && CP::stepIndex (0.49f, 2) == 0 && CP::stepIndex (0.5f, 0) == -1, "step index: halves, nothing to pick");
        expect (CP::bankCount (0) == 0 && CP::bankCount (7) == 1 && CP::bankCount (8) == 2 && CP::bankCount (10) == 2 && CP::bankCount (21) == 3,
                "bank count: seven controls per bank");
        expect (! CP::toggleOn (63) && CP::toggleOn (64), "toggle: on from CC 64");
        expect (CP::relativeDelta (65) == 1 && CP::relativeDelta (63) == -1 && CP::relativeDelta (66) == 2, "relative encoder: value - 64");
        expect (CP::relativeStep (65) == 1 && CP::relativeStep (66) == 1 && CP::relativeStep (61) == -1 && CP::relativeStep (64) == 0,
                "relative encoder: one step per message, whatever the acceleration");
        expect (CP::tempoAfterDetents (249.0, 3) == 250.0 && CP::tempoAfterDetents (41.0, -5) == 40.0 && CP::tempoAfterDetents (120.4, 1) == 121.0,
                "fine tempo: whole BPM, clamped 40..250");
        const juce::uint8 pcBody[] = { 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x02, 0x00, 0x40, 0x62, 0x01 };
        const auto programChange = juce::MidiMessage::createSysExMessage (pcBody, (int) sizeof (pcBody));
        const juce::uint8 otherBody[] = { 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x02, 0x00, 0x40, 0x63, 0x01 };
        const auto otherSysEx = juce::MidiMessage::createSysExMessage (otherBody, (int) sizeof (otherBody));
        expect (CP::isProgramChangeSysEx (programChange.getRawData(), programChange.getRawDataSize())
                && ! CP::isProgramChangeSysEx (otherSysEx.getRawData(), otherSysEx.getRawDataSize()),
                "the MiniLab's program-change SysEx is recognised, another one is not");

        CP::Profile mod;
        const bool modParsed = CP::parseText (R"({ "name": "Mod", "match": "Mod Box", "controls": [
              { "name": "k1",    "cc": 30, "param": "@module:1", "fallback": "filterCutoff", "takeover": "jump" },
              { "name": "k2",    "cc": 31, "param": "@module:2", "fallback": "nope", "takeover": "jump" },
              { "name": "k3",    "cc": 34, "param": "@module:3", "takeover": "jump" },
              { "name": "k5",    "cc": 35, "param": "@module:5", "takeover": "jump" },
              { "name": "k8",    "cc": 32, "param": "@module:8" },
              { "name": "k0",    "cc": 33, "param": "@module:0" },
              { "name": "what",  "cc": 36, "param": "@whatever" },
              { "name": "sel",   "cc": 40, "param": "@select" },
              { "name": "shift", "cc": 27, "param": "@shift" },
              { "name": "fine",  "cc": 29, "param": "@tempoFine" },
              { "name": "power", "cc": 42, "param": "@power" },
              { "name": "vol",   "cc": 41, "param": "masterVol", "takeover": "jump" } ] })", "fallback", exists, mod);
        expect (modParsed && mod.controls.size() == 9 && mod.controls[7].kind == CP::Kind::Power, "special targets parse without a parameter check; @module:8, @module:0, @whatever skipped");
        expect (mod.hasModuleTargets() && ! test.hasModuleTargets(), "a profile knows whether it has @module / @select entries");
        expect (mod.controls.size() == 9 && mod.controls[1].kind == CP::Kind::Module && mod.controls[1].moduleSlot == 2 && mod.controls[1].param.isEmpty(),
                "an unknown fallback drops only the fallback");
        {
            bool at = false;
            for (int n = 0; n < 128; ++n)
                for (const auto& t : CP::resolve (mod, 1, n, 0))
                    at = at || t.paramId.startsWithChar ('@') || t.paramId.isEmpty();
            expect (! at, "resolve never returns an @ target as a parameter id");
        }
        expect (ids (CP::resolve (mod, 1, 30, 0)) == "filterCutoff" && ids (CP::resolve (mod, 1, 30, 1)) == "bfilterCutoff",
                "nothing selected: @module:1 resolves to its fallback, by the focus");
        expect (ids (CP::resolve (mod, 1, 31, 0)).isEmpty() && ids (CP::resolve (mod, 1, 40, 0)).isEmpty()
                && ids (CP::resolve (mod, 1, 27, 0)).isEmpty() && ids (CP::resolve (mod, 1, 29, 0)).isEmpty()
                && ids (CP::resolve (mod, 1, 42, 0)).isEmpty(),
                "no fallback, @select, @shift, @tempoFine, @power: no fixed parameter");

        const CP::Profile* minilab = nullptr;
        const CP::Profile* generic = nullptr;
        for (const auto& p : shipped)
        {
            if (p.name == "MiniLab 3")      minilab = &p;
            if (p.name == CP::kGenericName) generic = &p;
        }
        expect (minilab != nullptr && generic != nullptr && minilab->hasModuleTargets() && ! generic->hasModuleTargets(),
                "MiniLab 3 has module targets, Generic (GM) has none");

        auto& apvts = r.p->getAPVTS();
        auto prm = [&] (const juce::String& id) { return apvts.getParameter (id); };
        auto raw = [&] (const juce::String& id) { auto* p = prm (id); return p != nullptr ? p->convertFrom0to1 (p->getValue()) : -999.0f; };
        using Ev = std::tuple<int, int, int>;   // channel, cc, value
        auto run = [&] (const CP::Profile& profile, std::initializer_list<Ev> events)
        {
            juce::MidiBuffer m;
            int pos = 0;
            for (const auto& [ch, num, val] : events)
                m.addEvent (juce::MidiMessage::controllerEvent (ch, num, val), pos++);
            r.p->runControllersForTest ({ profile }, m);
        };
        auto runMsg = [&] (const CP::Profile& profile, const juce::MidiMessage& msg)
        {
            juce::MidiBuffer m;
            m.addEvent (msg, 0);
            r.p->runControllersForTest ({ profile }, m);
        };
        auto sel = [&] { return r.p->getControllerSelection(); };
        // 22.8: nobody knows where the controls stand (as after a start) - the next message of each
        // only sets its reference.
        auto forget = [&] { r.p->controllerInputsChangedForTest(); };
        using Frame = SynthyProcessor::ControllerFrame;
        using MC = CP::ModuleControl;
        const juce::String bp (Modules::kLayerBPrefix);
        // FILTER as a frame with ten controls (two banks): knob, combo (4 items), toggle, knobs.
        auto filterFrame = [&] (bool layerB, int count = 10)
        {
            const juce::String pre = layerB ? bp : juce::String();
            Frame f { "filter", layerB, {
                { pre + "filterCutoff", CP::ControlKind::Knob, 0 }, { pre + "noiseType", CP::ControlKind::Combo, 4 },
                { pre + "filterOn", CP::ControlKind::Toggle, 0 },   { pre + "filterReso", CP::ControlKind::Knob, 0 },
                { pre + "attack", CP::ControlKind::Knob, 0 },       { pre + "decay", CP::ControlKind::Knob, 0 },
                { pre + "sustain", CP::ControlKind::Knob, 0 },      { pre + "release", CP::ControlKind::Knob, 0 },
                { pre + "lfo1Rate", CP::ControlKind::Knob, 0 },     { pre + "lfo1Depth", CP::ControlKind::Knob, 0 } } };
            f.controls.resize ((size_t) count);
            for (size_t i = 0; i < f.controls.size(); ++i)
                f.controls[i].bank = (int) i / CP::kBankSize;   // banks of seven, as the rack assigns them
            return f;
        };
        // Shift + knob 8 turned right by one bank step (the first message is only the reference).
        auto nextBank = [&]
        {
            run (mod, { Ev { 1, 27, 127 } });
            run (mod, { Ev { 1, 40, 60 } });
            run (mod, { Ev { 1, 40, 60 + CP::kBankStep } });
            run (mod, { Ev { 1, 27, 0 } });
        };
        auto one = [] (const char* entry, const char* id) { return Frame { entry, false, { MC { id, CP::ControlKind::Knob, 0 } } }; };
        auto sixFrames = [&]
        {
            return std::vector<Frame> { filterFrame (false), one ("amp", "attack"), one ("lfo1", "lfo1Rate"), Frame { "caption", false, {} },
                                        one ("chorus", "chorusMix"), one ("delay", "delayMix"), one ("reverb", "reverbMix") };
        };
        expect (prm ("noiseType") != nullptr && prm ("noiseType")->getNumSteps() == 4 && prm ("filterOn") != nullptr && prm ("release") != nullptr,
                "test parameters exist (noiseType has 4 items)");

        // Profile gating: no selection while the active profile has no @module / @select entry.
        run (test, {});
        r.p->setControllerFrames (sixFrames());
        expect (! r.p->controllerProfileSelectsModules() && ! r.p->selectControllerModule ("filter", false) && ! sel().valid,
                "profile without module targets: a click selects nothing");

        // Nothing selected: the fallback moves, by the focus (22.1 rules).
        run (mod, {});
        setup (0.3f, 0.2f, 0);
        run (mod, { Ev { 1, 30, 127 } });
        expect (! sel().valid && cutoff->getValue() == 1.0f && near (cutoffB->getValue(), 0.2f), "nothing selected, focus A: knob 1 moves its fallback filterCutoff");
        setup (0.3f, 0.2f, 1);
        run (mod, { Ev { 1, 30, 127 } });
        expect (near (cutoff->getValue(), 0.3f) && cutoffB->getValue() == 1.0f, "nothing selected, focus B: the fallback's b id");
        r.set (Parameters::ID::layerFocus, 0.0f);

        // Knob 8 (22.8) is relative: one module per kBankStep of travel, from the selected module -
        // its position never names one. Six selectable frames (the one without controls is none).
        forget();
        run (mod, { Ev { 1, 40, 0 } });
        expect (! sel().valid, "knob 8, first message (at 0): only the reference - not the first module");
        run (mod, { Ev { 1, 40, CP::kBankStep } });
        expect (sel().valid && sel().entryId == "filter" && ! sel().layerB && sel().bank == 0 && sel().bankCount == 2,
                "nothing selected, knob 8 a step right: the first module, bank 1 of 2");
        run (mod, { Ev { 1, 40, CP::kBankStep + 1 } });
        const bool lessThanAStep = sel().entryId == "filter";
        run (mod, { Ev { 1, 40, 2 * CP::kBankStep } });
        expect (lessThanAStep && sel().entryId == "amp", "FILTER selected: less than a step moves nothing, the full step right is the next module in reading order");
        run (mod, { Ev { 1, 40, 127 } });
        expect (sel().valid && sel().entryId == "reverb" && sel().bankCount == 1, "turned far right: clamped at the last of 6 selectable modules");
        for (int i = 0; i < 3; ++i)
            run (mod, { Ev { 1, 40, 127 } });
        expect (sel().entryId == "reverb", "knob 8 at its end, turned further (127 repeated): stays on the last module, no wrap");
        run (mod, { Ev { 1, 40, 127 - CP::kBankStep } });
        expect (sel().entryId == "delay", "a step left: the previous module");
        r.p->clearControllerSelection();
        run (mod, { Ev { 1, 40, 127 - 2 * CP::kBankStep } });
        expect (sel().valid && sel().entryId == "reverb", "nothing selected, knob 8 a step left: the last module");
        {
            const int rev = r.p->getControllerSelectionRevision();
            expect (r.p->selectControllerModule ("filter", false) && sel().entryId == "filter" && r.p->getControllerSelectionRevision() != rev,
                    "a click selects (profile with module targets)");
            run (mod, { Ev { 1, 40, 127 - 2 * CP::kBankStep + 1 } });
            const bool mouseKept = sel().entryId == "filter";
            run (mod, { Ev { 1, 40, 127 - CP::kBankStep } });
            expect (mouseKept && sel().entryId == "amp",
                    "after a mouse selection knob 8 goes on from that module (FILTER -> AMP), never to the module its position would name");
            // Nothing selected and a fast turn (more than two steps in one tick): exactly the
            // first module going right, the last going left - not the second.
            r.p->clearControllerSelection();
            forget();
            run (mod, { Ev { 1, 40, 40 } });
            run (mod, { Ev { 1, 40, 40 + 2 * CP::kBankStep + 6 } });
            const bool fastRight = sel().valid && sel().entryId == "filter";
            r.p->clearControllerSelection();
            run (mod, { Ev { 1, 40, 40 } });
            expect (fastRight && sel().valid && sel().entryId == "reverb", "nothing selected, knob 8 turned fast: the first module going right, the last going left");
            // Shift -> plain handover with a middle module selected: wherever Shift + knob 8 left
            // the knob (right or left of its last plain position), nothing steps by itself.
            r.p->selectControllerModule ("lfo1", false);
            run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 110 }, Ev { 1, 27, 0 } });
            run (mod, { Ev { 1, 40, 110 } });
            const bool handoverRight = sel().entryId == "lfo1";
            run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 5 }, Ev { 1, 27, 0 } });
            run (mod, { Ev { 1, 40, 5 } });
            const bool handoverLeft = sel().entryId == "lfo1";
            // Shift released inside the tick: the plain value counts from the Shift position.
            run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 90 }, Ev { 1, 27, 0 }, Ev { 1, 40, 91 } });
            expect (handoverRight && handoverLeft && sel().entryId == "lfo1",
                    "LFO 1 selected, Shift + knob 8 far right / far left, then plain: the travel made under Shift is no module travel - in the same tick either");
            // The knob at its end has to go on: 14 modules, more than its travel reaches.
            {
                std::vector<Frame> many;
                for (int i = 0; i < 14; ++i)
                    many.push_back (Frame { "m" + juce::String (i), false, { MC { "attack", CP::ControlKind::Knob, 0 } } });
                r.p->setControllerFrames (many);
                const int ticks = CP::kBankStep / CP::kBankEndTravel;
                forget();
                run (mod, { Ev { 1, 40, 127 - 2 * CP::kBankStep } });
                r.p->selectControllerModule ("m3", false);
                run (mod, { Ev { 1, 40, 127 } });
                const bool twoRight = sel().entryId == "m5";
                for (int i = 0; i < ticks - 1; ++i)
                    run (mod, { Ev { 1, 40, 127 } });
                const bool notYetRight = sel().entryId == "m5";
                run (mod, { Ev { 1, 40, 127 } });
                const bool endRight = sel().entryId == "m6";
                forget();
                run (mod, { Ev { 1, 40, 2 * CP::kBankStep } });
                r.p->selectControllerModule ("m8", false);
                run (mod, { Ev { 1, 40, 0 } });
                const bool twoLeft = sel().entryId == "m6";
                for (int i = 0; i < ticks; ++i)
                    run (mod, { Ev { 1, 40, 0 } });
                expect (twoRight && notYetRight && endRight && twoLeft && sel().entryId == "m5",
                        "knob 8 at its end, turned further (127 / 0 repeated): a bank step of end travel is the next / previous module");
                r.p->setControllerFrames (sixFrames());
                forget();
            }
            r.p->selectControllerModule ("filter", false);
            expect (! r.p->selectControllerModule ("nowhere", false) && ! r.p->selectControllerModule ("filter", true) && sel().entryId == "filter",
                    "a frame that is not on screen cannot be selected");
        }

        // Knobs 1..7 on the selected module: knob, combo, toggle; no fallback while selected.
        setup (0.3f, 0.2f, 0);
        run (mod, { Ev { 1, 30, 127 } });
        expect (cutoff->getValue() == 1.0f, "FILTER selected: knob 1 = 127 -> its 1st control at max");
        r.set ("noiseType", 0.0f);
        run (mod, { Ev { 1, 31, 100 } });
        expect (raw ("noiseType") == 3.0f, "2nd control, a combo with 4 items, knob at 100: item 3");
        run (mod, { Ev { 1, 31, 40 } });
        expect (raw ("noiseType") == 1.0f, "combo, knob at 40: item 1");
        run (mod, { Ev { 1, 34, 63 } });
        const bool toggleOff = raw ("filterOn") < 0.5f;
        run (mod, { Ev { 1, 34, 64 } });
        expect (toggleOff && raw ("filterOn") > 0.5f, "3rd control, a toggle: off at 63, on at 64");

        // Banks: Shift held + knob 8, relative — the knob's position never picks a bank.
        r.set ("release", 0.0f);
        r.set ("attack", 0.0f);
        const float release0 = prm ("release")->getValue(), attack0 = prm ("attack")->getValue();
        run (mod, { Ev { 1, 27, 127 } });
        expect (r.p->controllerShiftHeldForTest(), "Shift CC 127: held");
        run (mod, { Ev { 1, 40, 127 } });
        expect (sel().entryId == "filter" && sel().bank == 0 && sel().bankCount == 2,
                "Shift + knob 8, first message (knob at 127): only the reference - no jump to the last bank");
        run (mod, { Ev { 1, 40, 127 } });
        const int bankAfterOneRepeat = sel().bank;
        run (mod, { Ev { 1, 40, 127 } });
        run (mod, { Ev { 1, 40, 127 } });
        expect (bankAfterOneRepeat == 0 && sel().bank == 1, "knob at its end, turned further (127 repeated): keeps advancing, bank 2 after a step of travel");
        for (int i = 0; i < 6; ++i)
            run (mod, { Ev { 1, 40, 127 } });
        expect (sel().bank == 1, "clamped at the last bank, no wrap");
        run (mod, { Ev { 1, 40, 127 - CP::kBankStep } });
        expect (sel().bank == 0, "a step of travel left: the previous bank");
        run (mod, { Ev { 1, 40, 127 - 3 * CP::kBankStep } });
        expect (sel().bank == 0, "clamped at the first bank");
        run (mod, { Ev { 1, 40, 127 - 3 * CP::kBankStep + CP::kBankStep - 1 } });
        const int bankBeforeStep = sel().bank;
        run (mod, { Ev { 1, 40, 127 - 2 * CP::kBankStep } });
        expect (bankBeforeStep == 0 && sel().bank == 1 && sel().entryId == "filter", "less than a step: no move; the full step right: the next bank, the module stays");
        run (mod, { Ev { 1, 27, 0 } });
        expect (! r.p->controllerShiftHeldForTest(), "Shift CC 0: released");
        setup (0.3f, 0.2f, 0);
        run (mod, { Ev { 1, 30, 127 } });
        expect (prm ("release")->getValue() == 1.0f && release0 < 1.0f && near (cutoff->getValue(), 0.3f), "bank 2: knob 1 moves control 8");
        run (mod, { Ev { 1, 35, 127 } });
        expect (prm ("attack")->getValue() == attack0, "bank 2 has 3 controls: knob 5 moves nothing");
        expect (r.p->selectControllerModule ("filter", false) && sel().bank == 1, "touching the selected module again keeps its bank");
        // A short Shift press inside one tick: the knob-8 value sent under Shift is no module
        // jump — and, being the first under this Shift, no bank move either.
        run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 0 }, Ev { 1, 27, 0 } });
        expect (sel().entryId == "filter" && sel().bank == 1 && ! r.p->controllerShiftHeldForTest(), "Shift press + knob 8 + release in one tick: no module jump, the bank stays");
        run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 127 }, Ev { 1, 27, 0 } });
        expect (sel().entryId == "filter" && sel().bank == 1, "the same with knob 8 = 127: still FILTER (not the last module), still bank 2 (a new Shift press starts a new reference)");
        run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 64 }, Ev { 1, 27, 0 } });
        run (mod, { Ev { 1, 40, 64 } });
        expect (sel().entryId == "filter" && sel().bank == 1, "knob 8 left where Shift + knob 8 put it: its first plain message does not step to another module");
        // In the DAW program the selector's CC comes after the knobs: it is applied first all the same.
        r.p->selectControllerModule ("amp", false);
        r.set ("attack", 0.0f);
        setup (0.3f, 0.2f, 0);
        run (mod, { Ev { 1, 30, 127 }, Ev { 1, 40, 64 - CP::kBankStep } });   // a step left of AMP: FILTER
        expect (sel().entryId == "filter" && sel().bank == 0 && cutoff->getValue() == 1.0f && prm ("attack")->getValue() == attack0,
                "selector and knob 1 in one tick: the module is picked first, the bank starts at 1");

        // A lost Shift release.
        run (mod, { Ev { 1, 27, 127 } });
        runMsg (mod, otherSysEx);
        const bool stillHeld = r.p->controllerShiftHeldForTest();
        runMsg (mod, programChange);
        expect (stillHeld && ! r.p->controllerShiftHeldForTest(), "program-change SysEx releases Shift, another SysEx does not");
        run (mod, { Ev { 1, 40, 127 - 2 * CP::kBankStep } });
        const bool onlyReference = sel().entryId == "filter";
        run (mod, { Ev { 1, 40, 127 } });
        expect (onlyReference && sel().entryId == "lfo1",
                "after the SysEx knob 8 selects modules again - its first message only the reference (22.8: the program's positions are unknown), then two steps right");
        run (mod, { Ev { 1, 27, 127 } });
        r.p->controllerInputsChangedForTest();
        expect (! r.p->controllerShiftHeldForTest(), "MIDI inputs changed: Shift released");
        run (mod, { Ev { 1, 27, 127 } });
        run (test, {});
        expect (! r.p->controllerShiftHeldForTest() && ! sel().valid, "profile without @shift / module targets picked: Shift released, selection dropped");
        {
            // A channel-bound Shift listens on its channel only.
            CP::Profile shiftCh;
            CP::parseText (R"({ "name": "ShiftCh", "controls": [ { "name": "shift", "cc": 27, "channel": 2, "param": "@shift" } ] })",
                           "fallback", exists, shiftCh);
            run (shiftCh, { Ev { 2, 27, 127 } });
            const bool heldOn2 = r.p->controllerShiftHeldForTest();
            run (shiftCh, { Ev { 2, 27, 0 } });
            run (shiftCh, { Ev { 1, 27, 127 } });
            expect (shiftCh.controls.size() == 1 && heldOn2 && ! r.p->controllerShiftHeldForTest(),
                    "@shift bound to channel 2: held by its CC on channel 2, not by the same CC on channel 1");
        }
        run (mod, {});
        r.p->clearControllerSelection();
        run (mod, { Ev { 1, 27, 127 }, Ev { 1, 40, 127 } });
        for (int i = 0; i < 3; ++i)
            run (mod, { Ev { 1, 40, 127 } });   // a bank step of travel at the knob's end
        expect (! sel().valid, "Shift + knob 8 with nothing selected: ignored");
        run (mod, { Ev { 1, 27, 0 } });

        // What the rack shows changes: the selection stays while the module is still on screen.
        r.p->selectControllerModule ("filter", false);
        nextBank();
        {
            auto frames = sixFrames();
            frames[0] = filterFrame (true);
            r.p->setControllerFrames (frames);
            expect (sel().valid && sel().entryId == "filter" && sel().layerB && sel().bank == 1, "frame swap A -> B: FILTER stays selected, bank kept");
            r.set ((bp + "release").toRawUTF8(), 0.0f);
            r.set ("release", 0.0f);
            run (mod, { Ev { 1, 30, 127 } });
            expect (prm (bp + "release")->getValue() == 1.0f && prm ("release")->getValue() < 1.0f, "after the swap knob 1 moves the b id");
            frames[0] = filterFrame (true, 3);
            r.p->setControllerFrames (frames);
            expect (sel().valid && sel().bank == 0 && sel().bankCount == 1, "fewer controls: the bank is clamped");
            frames.erase (frames.begin());
            r.p->setControllerFrames (frames);
            expect (! sel().valid, "module no longer shown: selection dropped");
            setup (0.3f, 0.2f, 0);
            run (mod, { Ev { 1, 30, 127 } });
            expect (cutoff->getValue() == 1.0f, "selection dropped: knob 1 is back on its fallback");
        }
        r.p->setControllerFrames (sixFrames());
        r.p->selectControllerModule ("filter", false);
        nextBank();
        const int bankBeforeReselect = sel().bank;
        r.p->selectControllerModule ("amp", false);
        r.p->selectControllerModule ("filter", false);
        expect (bankBeforeReselect == 1 && sel().bank == 0, "a new selection starts at bank 1");
        {
            const int rev = r.p->getControllerSelectionRevision();
            auto frames = sixFrames();
            frames[0] = filterFrame (false, 3);
            r.p->setControllerFrames (frames);
            expect (sel().valid && sel().bank == 0 && sel().bankCount == 1 && r.p->getControllerSelectionRevision() != rev,
                    "only the bank count changed: the selection revision bumps all the same");
            r.p->setControllerFrames (sixFrames());
        }
        // An A frame in A+B keeps linked editing.
        setup (0.6f, 0.2f, 2);
        {
            const float a0 = cutoff->getValue(), b0 = cutoffB->getValue();
            run (mod, { Ev { 1, 30, 127 } });
            expect (cutoff->getValue() == 1.0f && near (cutoffB->getValue(), b0 + (1.0f - a0)), "A frame selected in A+B: B follows by linked editing");
        }
        r.set (Parameters::ID::layerFocus, 0.0f);
        r.p->clearControllerSelection();
        expect (! sel().valid, "clearControllerSelection: nothing selected");

        //------------------------------------------------------------------------------------------
        // Story 22.8: soft takeover - a control never writes its position, it moves the value it
        // finds. Every row of the spec's matrix; "soft" has encoder knobs, a fader and the opt-out.
        std::cout << "Controller profiles: soft takeover (22.8)" << std::endl;
        CP::Profile soft;
        {
            using T = CP::Takeover;
            expect (CP::encoderDelta (-1, 101, 2) == 0 && CP::encoderDelta (101, 111, 2) == 10 && CP::encoderDelta (111, 90, 2) == -21,
                    "encoder: the first message is no travel, then the difference");
            expect (CP::encoderDelta (127, 127, 2) == 2 && CP::encoderDelta (0, 0, 2) == -2 && CP::encoderDelta (50, 50, 2) == 0,
                    "encoder: a repeated 127 / 0 is travel at the end, a repeated value elsewhere is none");
            expect (tight (CP::scaledValue (0.5f, 0.2f, 0.6f), 0.75f) && tight (CP::scaledValue (0.5f, 0.6f, 0.3f), 0.25f),
                    "scale: fader 0.2 -> 0.6 (half its way up) takes 0.5 to 0.75; 0.6 -> 0.3 (half its way down) halves it");
            expect (CP::scaledValue (0.5f, 0.2f, 1.0f) == 1.0f && CP::scaledValue (0.5f, 0.2f, 0.0f) == 0.0f && CP::scaledValue (0.5f, 0.4f, 0.4f) == 0.5f
                    && CP::scaledValue (0.3f, 1.0f, 1.0f) == 0.3f && CP::scaledValue (0.3f, 0.0f, 0.0f) == 0.3f,
                    "scale: fader top / bottom = exactly max / min; a fader that did not move moves nothing, at an end either");
            expect (tight (CP::takenOver (T::Encoder, 0.3f, 101, 111), 0.3f + 10.0f * u) && CP::takenOver (T::Encoder, 0.3f, -1, 101) == 0.3f
                    && CP::takenOver (T::Scale, 0.5f, -1, 25) == 0.5f && tight (CP::takenOver (T::Jump, 0.5f, -1, 100), 100.0f * u)
                    && CP::takenOver (T::Encoder, 0.99f, 0, 127) == 1.0f,
                    "takeover: the first message moves nothing (encoder, scale); jump is the position; clamped at the top");
            float travel = 0.0f;
            const int s1 = CP::takeSteps (travel, 11.0f, 12.0f), s2 = CP::takeSteps (travel, 1.0f, 12.0f), s3 = CP::takeSteps (travel, 11.0f, 12.0f);
            const int s4 = CP::takeSteps (travel, -11.0f, 12.0f), s5 = CP::takeSteps (travel, -25.0f, 12.0f);
            expect (s1 == 0 && s2 == 1 && s3 == 0 && s4 == 0 && s5 == -3, "stepped travel: whole steps only, the rest is kept, a turn back counts afresh");
            travel = 0.0f;
            const int q1 = CP::steppedIndex (T::Encoder, 0, 4, 10, 41, travel), q2 = CP::steppedIndex (T::Encoder, 0, 4, 41, 42, travel);
            expect (q1 == 0 && q2 == 1, "encoder on a combo with 4 items: a quarter of the travel (32 of 127) is one item");
            travel = 0.0f;
            const int w1 = CP::steppedIndex (T::Encoder, 0, 2, 60, 71, travel), w2 = CP::steppedIndex (T::Encoder, 0, 2, 71, 72, travel);
            const int w3 = CP::steppedIndex (T::Encoder, 1, 2, 72, 60, travel);
            expect (w1 == 0 && w2 == 1 && w3 == 0, "encoder on anything with two steps (a switch): a clear turn (a bank step) right is on, left is off");
            expect (CP::steppedIndex (T::Jump, 0, 2, -1, 63, travel) == 0 && CP::steppedIndex (T::Jump, 0, 2, -1, 64, travel) == 1, "jump on a switch: on from CC 64, as in 22.5");
            travel = 0.0f;
            expect (CP::steppedIndex (T::Scale, 1, 4, 50, 127, travel) == 3 && CP::steppedIndex (T::Scale, 2, 4, 50, 0, travel) == 0
                    && CP::steppedIndex (T::Scale, 2, 4, 127, 127, travel) == 2 && CP::steppedIndex (T::Encoder, 2, 4, -1, 90, travel) == 2
                    && CP::steppedIndex (T::Jump, 0, 4, -1, 100, travel) == 3 && CP::steppedIndex (T::Encoder, 0, 0, 1, 2, travel) == -1,
                    "stepped: fader top / bottom = last / first item, a repeated message and a first message move nothing, jump is the position, no items: nothing");
            travel = 0.0f;
            const int big1 = CP::steppedIndex (T::Encoder, 10, 300, 50, 51, travel), big2 = CP::steppedIndex (T::Encoder, 11, 300, 51, 49, travel);
            expect (big1 == 11 && big2 == 9, "more than 127 items: one CC unit is one item, never two");
            {
                // A control with ends at rest flickers by one unit: no move. The ends always count.
                int ref = -1, dir = 0;
                const int first = CP::scaleReference (ref, 50, dir), up = CP::scaleReference (ref, 60, dir);
                bool flickerIgnored = true;
                for (int i = 0; i < 20; ++i)
                    flickerIgnored = flickerIgnored && CP::scaleReference (ref, i % 2 == 0 ? 59 : 60, dir) == (i % 2 == 0 ? 59 : 60);
                const int onward = CP::scaleReference (ref, 61, dir), back = CP::scaleReference (ref, 59, dir);
                int endRef = 1, endDir = 1;
                const int bottom = CP::scaleReference (endRef, 0, endDir);
                expect (first == -1 && up == 50 && flickerIgnored && onward == 60 && back == 61 && ref == 59 && dir == -1 && bottom == 1 && endRef == 0,
                        "scale: one unit against the last direction is no move (the reference stays), a unit onward and a real move back are, the bottom always counts");
            }

            const bool softParsed = CP::parseText (R"({ "name": "Soft", "match": "Soft Box", "controls": [
                  { "name": "k1",   "cc": 30, "param": "@module:1", "fallback": "filterCutoff", "takeover": "encoder" },
                  { "name": "k2",   "cc": 31, "param": "@module:2", "takeover": "encoder" },
                  { "name": "k3",   "cc": 34, "param": "@module:3", "takeover": " Encoder " },
                  { "name": "sel",  "cc": 40, "param": "@select" },
                  { "name": "vol",  "cc": 41, "param": "masterVol" },
                  { "name": "f2",   "cc": 46, "param": "@module:2", "takeover": "scale" },
                  { "name": "odd",  "cc": 44, "param": "masterVol", "takeover": "pickup" },
                  { "name": "jump", "cc": 45, "param": "masterVol", "takeover": "jump" },
                  { "name": "pinA", "cc": 47, "param": "filterCutoff", "layer": "A" },
                  { "name": "type", "cc": 48, "param": "filterType", "takeover": "encoder" } ] })", "fallback", exists, soft);
            expect (softParsed && soft.controls.size() == 10 && soft.controls[0].takeover == T::Encoder && soft.controls[2].takeover == T::Encoder
                    && soft.controls[4].takeover == T::Scale && soft.controls[5].takeover == T::Scale && soft.controls[7].takeover == T::Jump,
                    "takeover parses: encoder (case and spaces ignored), scale, jump; a missing field is scale");
            expect (soft.controls.size() == 10 && soft.controls[6].takeover == T::Scale && soft.controls[6].param == "masterVol",
                    "an unknown takeover word keeps the entry, as scale");
            if (minilab != nullptr && generic != nullptr)
            {
                int encoders = 0, scaled = 0;
                bool rightKinds = true;
                for (const auto& c : minilab->controls)
                {
                    if (c.takeover == T::Encoder) { ++encoders; rightKinds = rightKinds && c.kind == CP::Kind::Module; }
                    if (c.kind == CP::Kind::Param)  { ++scaled;   rightKinds = rightKinds && c.takeover == T::Scale; }
                }
                bool genericScale = true;
                for (const auto& c : generic->controls)
                    genericScale = genericScale && c.takeover == T::Scale;
                expect (encoders == 14 && scaled == 8 && rightKinds && genericScale,
                        "MiniLab 3: the 14 entries of knobs 1..7 are encoders (knob 8, the selector, is relative by itself), the 8 fader entries scale; Generic (GM): all scale");
            }

            auto* vol = prm (Parameters::ID::masterVol);
            const float volBefore = vol->getValue();
            run (soft, {});
            r.set (Parameters::ID::layerFocus, 0.0f);
            r.p->setControllerFrames (sixFrames());
            r.p->selectControllerModule ("filter", false);

            // Encoder: first touch, turn, at its end, the mouse in between.
            forget();
            cutoff->setValueNotifyingHost (0.3f);
            const float c0 = cutoff->getValue();
            run (soft, { Ev { 1, 30, 101 } });
            expect (cutoff->getValue() == c0, "encoder, first touch (knob at 101, Cutoff at 0.30): Cutoff stays");
            run (soft, { Ev { 1, 30, 111 } });
            expect (tight (cutoff->getValue(), c0 + 10.0f * u), "encoder, 101 -> 111: Cutoff + 10/127");
            run (soft, { Ev { 1, 30, 127 } });
            const float atEnd = cutoff->getValue();
            run (soft, { Ev { 1, 30, 127 } });
            const float oneTick = cutoff->getValue();
            run (soft, { Ev { 1, 30, 127 } });
            expect (tight (atEnd, c0 + 26.0f * u) && tight (oneTick, atEnd + (float) CP::kEncoderEndTravel * u)
                    && tight (cutoff->getValue(), atEnd + 2.0f * (float) CP::kEncoderEndTravel * u),
                    "encoder at its end (127 repeated): the value keeps rising a little per tick");
            cutoff->setValueNotifyingHost (0.2f);
            const float m0 = cutoff->getValue();
            run (soft, { Ev { 1, 30, 120 } });
            expect (tight (cutoff->getValue(), m0 - 7.0f * u), "value changed by the mouse, then the knob turned: it goes on from the mouse's value");

            // Module change: the knob moves the new module's control from that control's own value.
            r.p->selectControllerModule ("amp", false);
            prm ("attack")->setValueNotifyingHost (0.4f);
            const float a0 = prm ("attack")->getValue(), c1 = cutoff->getValue();
            run (soft, { Ev { 1, 30, 125 } });
            expect (tight (prm ("attack")->getValue(), a0 + 5.0f * u) && cutoff->getValue() == c1,
                    "another module selected, knob 1 + 5: its 1st control rises by 5/127 from its own value, FILTER's stays");

            // Encoder on a combo and on a switch.
            r.p->selectControllerModule ("filter", false);
            r.set ("noiseType", 0.0f);
            forget();
            run (soft, { Ev { 1, 31, 10 } });
            run (soft, { Ev { 1, 31, 41 } });
            const bool notYet = raw ("noiseType") == 0.0f;
            run (soft, { Ev { 1, 31, 42 } });
            const bool nextItem = raw ("noiseType") == 1.0f;
            run (soft, { Ev { 1, 31, 10 } });
            expect (notYet && nextItem && raw ("noiseType") == 0.0f,
                    "encoder on a combo with 4 items: a quarter of the travel right is the next item, the same way back the previous one");
            r.set ("filterOn", 0.0f);
            run (soft, { Ev { 1, 34, 60 } });
            run (soft, { Ev { 1, 34, 60 + CP::kBankStep - 1 } });
            const bool stillOff = raw ("filterOn") < 0.5f;
            run (soft, { Ev { 1, 34, 60 + CP::kBankStep } });
            const bool nowOn = raw ("filterOn") > 0.5f;
            run (soft, { Ev { 1, 34, 90 } });
            const bool staysOn = raw ("filterOn") > 0.5f;
            run (soft, { Ev { 1, 34, 90 - CP::kBankStep } });
            expect (stillOff && nowOn && staysOn && raw ("filterOn") < 0.5f,
                    "encoder on a switch: a clear turn right is on (further right stays on), a clear turn left is off");

            // The travel belongs to its target: begun on one module's switch, it does not finish on
            // the next module's.
            {
                r.p->setControllerFrames ({ Frame { "x", false, { MC { "filterOn", CP::ControlKind::Toggle, 0 } } },
                                            Frame { "y", false, { MC { "bfilterOn", CP::ControlKind::Toggle, 0 } } } });
                r.set ("filterOn", 0.0f);
                r.set ("bfilterOn", 0.0f);
                r.p->selectControllerModule ("x", false);
                forget();
                run (soft, { Ev { 1, 30, 40 } });
                run (soft, { Ev { 1, 30, 40 + CP::kBankStep - 1 } });
                const bool xOff = raw ("filterOn") < 0.5f;
                r.p->selectControllerModule ("y", false);
                run (soft, { Ev { 1, 30, 40 + CP::kBankStep } });
                const bool yOff = raw ("bfilterOn") < 0.5f && raw ("filterOn") < 0.5f;
                run (soft, { Ev { 1, 30, 40 + 2 * CP::kBankStep } });
                expect (xOff && yOff && raw ("bfilterOn") > 0.5f,
                        "11 units on one module's switch, another module selected, 1 more unit: its switch does not flip - a full clear turn does");
                // The same switch, changed by the mouse in between: the travel starts afresh too.
                r.set ("bfilterOn", 0.0f);
                run (soft, { Ev { 1, 30, 40 + 3 * CP::kBankStep - 1 } });
                const bool elevenAgain = raw ("bfilterOn") < 0.5f;
                r.set ("bfilterOn", 1.0f);
                run (soft, { Ev { 1, 30, 40 + 3 * CP::kBankStep - 1 } });
                r.set ("bfilterOn", 0.0f);
                run (soft, { Ev { 1, 30, 40 + 3 * CP::kBankStep } });
                expect (elevenAgain && raw ("bfilterOn") < 0.5f, "a switch changed by the mouse in between: the travel made before does not count");
                r.set ("bfilterOn", 0.0f);
            }
            // A list whose parameter holds a value the list does not show: its first item must be
            // reachable (by any real move), but not by the control's first message.
            {
                MC list;
                list.paramId = "noiseType";
                list.kind = CP::ControlKind::List;
                list.listCount = [] { return 2; };
                list.listValues = [] { return juce::Array<int> { 2, 3 }; };
                r.p->setControllerFrames ({ Frame { "list", false, { list } } });
                r.p->selectControllerModule ("list", false);
                r.set ("noiseType", 0.0f);
                forget();
                run (soft, { Ev { 1, 30, 60 } });
                const bool listFirstTouch = raw ("noiseType") == 0.0f;
                run (soft, { Ev { 1, 30, 59 } });
                expect (listFirstTouch && raw ("noiseType") == 2.0f,
                        "list that does not hold the parameter's value: the first message moves nothing, a turn left picks the first item");
                r.set ("noiseType", 0.0f);
            }
            // A program change of the device (its SysEx): the positions are unknown again.
            {
                r.p->setControllerFrames (sixFrames());
                r.p->selectControllerModule ("filter", false);
                run (soft, { Ev { 1, 30, 50 } });
                cutoff->setValueNotifyingHost (0.3f);
                const float p0 = cutoff->getValue();
                runMsg (soft, programChange);
                run (soft, { Ev { 1, 30, 90 } });
                const bool forgotten = cutoff->getValue() == p0;
                run (soft, { Ev { 1, 30, 95 } });
                expect (forgotten && tight (cutoff->getValue(), p0 + 5.0f * u),
                        "program-change SysEx: a knob's next message only says where it stands in the new program, then it moves the value by its turn");
            }
            // A parameter that snaps: one CC unit per tick must add up, not be lost each time.
            {
                cutoff->setValueNotifyingHost (0.0f);
                forget();
                for (int v = 40; v <= 50; ++v)
                    run (soft, { Ev { 1, 30, v } });
                expect (near (cutoff->getValue(), 10.0f * u), "Cutoff at its range start, knob 1 turned by 10 single units: it has left the start by about 10/127");
            }
            // A fixed entry on a parameter with exactly two steps (a choice): a switch like any other.
            {
                r.set ("filterType", 0.0f);
                forget();
                run (soft, { Ev { 1, 48, 60 } });
                run (soft, { Ev { 1, 48, 60 + CP::kBankStep - 1 } });
                const bool typeStays = raw ("filterType") == 0.0f;
                run (soft, { Ev { 1, 48, 60 + CP::kBankStep } });
                expect (typeStays && raw ("filterType") == 1.0f, "fixed entry on a two-item choice: a clear turn right is its second item, as on a switch of a module");
                r.set ("filterType", 0.0f);
            }

            // A knob on a parameter with few steps (an integer, a choice) steps it - a CC unit at a
            // time would snap back on every message.
            r.p->setControllerFrames ({ Frame { "noise", false, { MC { "noiseType", CP::ControlKind::Knob, 0 } } } });
            r.p->selectControllerModule ("noise", false);
            r.set ("noiseType", 1.0f);
            forget();
            run (soft, { Ev { 1, 30, 60 } });
            run (soft, { Ev { 1, 30, 92 } });
            const bool stepped = raw ("noiseType") == 2.0f;
            run (soft, { Ev { 1, 30, 91 } });
            expect (stepped && raw ("noiseType") == 2.0f, "encoder on a knob whose parameter has 4 steps: a quarter of the travel is one step");

            // A control with ends ("scale") on a combo: it meets the list at its ends.
            r.p->setControllerFrames (sixFrames());
            r.p->selectControllerModule ("filter", false);
            r.set ("noiseType", 1.0f);
            forget();
            run (soft, { Ev { 1, 46, 40 } });
            const bool comboStays = raw ("noiseType") == 1.0f;
            run (soft, { Ev { 1, 46, 127 } });
            const bool comboTop = raw ("noiseType") == 3.0f;
            run (soft, { Ev { 1, 46, 0 } });
            expect (comboStays && comboTop && raw ("noiseType") == 0.0f, "scale on a combo: first touch nothing, top = last item, bottom = first item");

            // Fader (the default, "scale"): first touch, up, down, the ends, a repeated message.
            vol->setValueNotifyingHost (0.5f);
            forget();
            run (soft, { Ev { 1, 41, 27 } });
            const bool volStays = vol->getValue() == 0.5f;
            run (soft, { Ev { 1, 41, 77 } });
            expect (volStays && tight (vol->getValue(), 0.75f), "fader, first touch: MASTER VOL 0.50 stays; then half of its remaining way up: 0.50 -> 0.75");
            vol->setValueNotifyingHost (0.5f);
            forget();
            run (soft, { Ev { 1, 41, 80 } });
            run (soft, { Ev { 1, 41, 40 } });
            expect (tight (vol->getValue(), 0.25f), "fader down by half of its way: MASTER VOL halves");
            run (soft, { Ev { 1, 41, 127 } });
            const bool volTop = vol->getValue() == 1.0f;
            run (soft, { Ev { 1, 41, 0 } });
            expect (volTop && vol->getValue() == 0.0f, "fader to the top / bottom: MASTER VOL exactly max / min");
            vol->setValueNotifyingHost (0.5f);
            run (soft, { Ev { 1, 41, 0 } });
            const bool repeatStays = vol->getValue() == 0.5f;
            run (soft, { Ev { 1, 41, 13 } });
            expect (repeatStays && near (vol->getValue(), 0.5f + 0.5f * 13.0f * u),
                    "fader resting at the bottom, value moved by the mouse: a repeated message moves nothing, a move up goes on from the mouse's value");

            // A fader at rest flickers by one unit: the value must not creep towards the fader.
            vol->setValueNotifyingHost (0.5f);
            forget();
            run (soft, { Ev { 1, 41, 90 } });
            run (soft, { Ev { 1, 41, 100 } });
            const float rested = vol->getValue();
            bool flickerStays = rested > 0.5f;
            for (int i = 0; i < 20; ++i)
            {
                run (soft, { Ev { 1, 41, i % 2 == 0 ? 99 : 100 } });
                flickerStays = flickerStays && vol->getValue() == rested;
            }
            run (soft, { Ev { 1, 41, 97 } });
            expect (flickerStays && vol->getValue() < rested, "fader at rest, 20 flickers of one unit: the value stays where it was; a real move of 3 units down moves it");

            // An unknown word behaves as scale; the opt-out is absolute as in 22.1.
            vol->setValueNotifyingHost (0.5f);
            forget();
            run (soft, { Ev { 1, 44, 100 } });
            const bool oddStays = vol->getValue() == 0.5f;
            run (soft, { Ev { 1, 45, 100 } });
            expect (oddStays && near (vol->getValue(), 100.0f * u), "unknown takeover word: first touch moves nothing (scale); \"takeover\": \"jump\": the position at once");

            // Nothing selected: the fallback, by the focus; in A+B linked editing moves B alike.
            r.p->clearControllerSelection();
            setup (0.3f, 0.2f, 1);
            forget();
            const float fa = cutoff->getValue(), fb = cutoffB->getValue();
            run (soft, { Ev { 1, 30, 50 } });
            run (soft, { Ev { 1, 30, 60 } });
            expect (cutoff->getValue() == fa && tight (cutoffB->getValue(), fb + 10.0f * u), "nothing selected, focus B: the knob moves the fallback's b id from its own value");
            setup (0.6f, 0.2f, 2);
            const float la = cutoff->getValue(), lb = cutoffB->getValue();
            run (soft, { Ev { 1, 30, 70 } });
            expect (tight (cutoff->getValue(), la + 10.0f * u) && tight (cutoffB->getValue(), lb + 10.0f * u), "A+B: A moves by the knob's turn, linked editing moves B by the same");
            setup (0.6f, 0.2f, 2);
            forget();
            const float pa = cutoff->getValue(), pb = cutoffB->getValue();
            run (soft, { Ev { 1, 47, 40 } });
            const bool pinFirst = cutoff->getValue() == pa && cutoffB->getValue() == pb;
            run (soft, { Ev { 1, 47, 127 } });
            expect (pinFirst && cutoff->getValue() == 1.0f && cutoffB->getValue() == pb, "A+B, an entry pinned to A with the default takeover: A only, B is not carried along");
            r.set (Parameters::ID::layerFocus, 0.0f);
            vol->setValueNotifyingHost (volBefore);
            run (mod, {});
        }

        // Fine tempo: one message per detent, none lost inside a tick.
        r.set (Parameters::ID::syncTempo, 120.0f);
        run (mod, { Ev { 1, 29, 65 }, Ev { 1, 29, 65 }, Ev { 1, 29, 65 } });
        expect (raw (Parameters::ID::syncTempo) == 123.0f, "Shift + encoder, 3 detents in one tick: tempo +3 BPM");
        run (mod, { Ev { 1, 29, 63 } });
        expect (raw (Parameters::ID::syncTempo) == 122.0f, "one detent down: -1 BPM");
        r.set (Parameters::ID::syncTempo, 249.0f);
        run (mod, { Ev { 1, 29, 66 } });
        expect (raw (Parameters::ID::syncTempo) == 250.0f, "up at 249: clamped to 250");
        r.set (Parameters::ID::syncTempo, 41.0f);
        run (mod, { Ev { 1, 29, 60 } });
        expect (raw (Parameters::ID::syncTempo) == 40.0f, "down at 41: clamped to 40");
        // Detents turned under a profile that does not map them are not inherited later.
        run (test, { Ev { 1, 29, 70 } });
        run (mod, { Ev { 1, 29, 65 } });
        expect (raw (Parameters::ID::syncTempo) == 41.0f, "unmapped detents are drained, not kept for a later profile");

        // The shipped MiniLab 3 layout: faders on the globals in every focus, both programs; knobs
        // on their fallback while nothing is selected; knob 8 no longer moves GLIDE.
        if (minilab != nullptr)
        {
            // Every knob's fallback id: an unknown one would only drop the fallback, not the entry.
            const std::pair<int, const char*> knobFallbacks[] = {
                { 74, "filterCutoff" }, { 71, "filterReso" }, { 76, "lfo1Rate" }, { 77, "lfo1Depth" },
                { 93, "chorusMix" },    { 18, "delayMix" },   { 19, "reverbMix" },
                { 86, "filterCutoff" }, { 87, "filterReso" }, { 89, "lfo1Rate" }, { 90, "lfo1Depth" },
                { 110, "chorusMix" },   { 111, "delayMix" },  { 116, "reverbMix" } };
            bool fallbacksOk = true;
            for (const auto& [knobCc, id] : knobFallbacks)
                fallbacksOk = fallbacksOk && ids (CP::resolve (*minilab, 1, knobCc, 0)) == id;
            expect (fallbacksOk, "MiniLab 3: knobs 1..7 of both programs fall back to the 22.1 parameters");
            const char* globals[] = { Parameters::ID::masterVol, Parameters::ID::syncTempo, Parameters::ID::layerALevel, Parameters::ID::layerBLevel };
            const int arturia[] = { 82, 83, 85, 17 }, daw[] = { 14, 15, 30, 31 };
            bool fadersOk = true;
            for (int focus = 0; focus < 3; ++focus)
                for (const int* ccs : { arturia, daw })
                    for (int i = 0; i < 4; ++i)
                    {
                        r.set (Parameters::ID::layerFocus, 0.0f);
                        setParam (*r.p, globals[i], prm (globals[i])->getNormalisableRange().start);
                        r.set (Parameters::ID::layerFocus, (float) focus);
                        forget();
                        run (*minilab, { Ev { 1, ccs[i], 64 } });
                        const bool stays = prm (globals[i])->getValue() == 0.0f;   // first touch
                        run (*minilab, { Ev { 1, ccs[i], 127 } });
                        fadersOk = fadersOk && stays && prm (globals[i])->getValue() == 1.0f;
                    }
            expect (fadersOk, "MiniLab 3: faders 1..4 move MASTER VOL, TEMPO, A LVL, B LVL in focus A, B and A+B, both programs - nothing on first touch, the maximum at the top");
            setup (0.3f, 0.2f, 0);
            forget();
            const float k0 = cutoff->getValue();
            run (*minilab, { Ev { 1, 74, 100 } });
            const bool arturiaFirst = cutoff->getValue() == k0;
            run (*minilab, { Ev { 1, 74, 110 } });
            const bool arturiaKnob = arturiaFirst && tight (cutoff->getValue(), k0 + 10.0f * u);
            setup (0.3f, 0.2f, 0);
            run (*minilab, { Ev { 1, 86, 100 } });
            const bool dawFirst = cutoff->getValue() == k0;
            run (*minilab, { Ev { 1, 86, 110 } });
            expect (arturiaKnob && dawFirst && tight (cutoff->getValue(), k0 + 10.0f * u),
                    "MiniLab 3, nothing selected: knob 1 (CC 74 / 86) moves FILTER Cutoff from its value by the knob's turn, nothing on first touch");
            const float glide0 = prm ("glideTime")->getValue();
            r.p->setControllerFrames ({});
            run (*minilab, { Ev { 1, 16, 0 }, Ev { 1, 117, 0 } });
            run (*minilab, { Ev { 1, 16, 127 }, Ev { 1, 117, 127 } });
            expect (prm ("glideTime")->getValue() == glide0 && ! sel().valid, "MiniLab 3: knob 8 no longer moves GLIDE; with no frame list it selects nothing");
            r.set (Parameters::ID::syncTempo, 120.0f);
            run (*minilab, { Ev { 1, 27, 127 }, Ev { 1, 29, 65 }, Ev { 1, 27, 0 } });
            expect (raw (Parameters::ID::syncTempo) == 121.0f, "MiniLab 3, DAW program: Shift + encoder (CC 29) = +1 BPM");
            {
                bool power = false;
                for (const auto& c : minilab->controls)
                    power = power || (c.kind == CP::Kind::Power && c.cc == 118);
                bool powerArturia = false;
                for (const auto& c : minilab->controls)
                    powerArturia = powerArturia || (c.kind == CP::Kind::Power && c.cc == 115);
                expect (powerArturia, "MiniLab 3, ARTURIA program: the encoder click (CC 115, measured 2026-11-28) is @power");
                expect (power, "MiniLab 3, DAW program: the encoder click (CC 118) is @power");
            }
            // Measured at the device 2026-11-27: turned fast it sends 66 / 61 (accelerated) — still one BPM per message.
            run (*minilab, { Ev { 1, 27, 127 }, Ev { 1, 29, 66 }, Ev { 1, 29, 66 }, Ev { 1, 27, 0 } });
            expect (raw (Parameters::ID::syncTempo) == 123.0f, "MiniLab 3: two accelerated messages (66) = +2 BPM, not +4");
            run (*minilab, { Ev { 1, 27, 127 }, Ev { 1, 29, 61 }, Ev { 1, 27, 0 } });
            expect (raw (Parameters::ID::syncTempo) == 122.0f, "MiniLab 3: one accelerated message down (61) = -1 BPM");
            // ARTURIA program: Shift is CC 9, Shift + encoder CC 112 and sends 64 then 65 per click.
            run (*minilab, { Ev { 1, 9, 127 } });
            expect (r.p->controllerShiftHeldForTest(), "MiniLab 3, ARTURIA program: Shift (CC 9) is held");
            run (*minilab, { Ev { 1, 112, 64 }, Ev { 1, 112, 65 }, Ev { 1, 9, 0 } });
            expect (raw (Parameters::ID::syncTempo) == 123.0f && ! r.p->controllerShiftHeldForTest(),
                    "MiniLab 3, ARTURIA program: Shift + encoder (CC 112: 64, 65) = +1 BPM");
            // Shift pressed in one program and released in the other (Shift + Pad 3 switches): the
            // release arrives on the other program's CC and must still release the one Shift.
            run (*minilab, { Ev { 1, 9, 127 } });
            const bool heldAcross = r.p->controllerShiftHeldForTest();
            run (*minilab, { Ev { 1, 27, 0 } });
            expect (heldAcross && ! r.p->controllerShiftHeldForTest(), "Shift pressed as CC 9, released as CC 27: released");
            r.set (Parameters::ID::layerFocus, 0.0f);
        }

        //------------------------------------------------------------------------------------------
        // Story 22.5, step b: what the knobs reach in a module is derived from its descriptor
        // (rack::countableControls) — the same function the rack's frames hand to the editor.
        std::cout << "Controller profiles: countable controls of a module (22.5, step b)" << std::endl;
        {
            using CC = rack::CountableControl;
            auto layerB = [&] (ModuleSpec m)   // the editor's LS: a patch module's spec with layer B's ids
            {
                for (auto& p : m.params) p.id = bp + p.id;
                if (m.enableParamId.isNotEmpty()) m.enableParamId = bp + m.enableParamId;
                for (auto& id : m.bodyOrder) if (id.isNotEmpty() && ! id.startsWith ("step:")) id = bp + id;
                return m;
            };
            auto allExist = [&] (const juce::StringArray& list) { bool ok = ! list.isEmpty(); for (const auto& id : list) ok = ok && exists (id); return ok; };

            // Without bodyOrder: the params in registration order, the enable bool is the header's.
            const auto filterDesc = makeModuleDescriptor (Modules::filter());
            const auto filterIds = rack::countableParamIds (filterDesc);
            expect (filterIds.joinIntoString (",") == "filterType,filterCutoff,filterReso,filterTrack" && allExist (filterIds),
                    "FILTER (no bodyOrder): TYPE, CUTOFF, RESO, TRACK in display order, the enable switch not counted");
            const auto filterControls = rack::countableControls (filterDesc);
            expect (filterControls.size() == 4 && filterControls[0].kind == CC::Kind::Combo && filterControls[0].items == 2
                    && filterControls[1].kind == CC::Kind::Knob,
                    "FILTER: a combo carries its item count, a knob is a knob");

            // With bodyOrder: display order, pattern cells ("step:n") and spacers skipped.
            const auto seqDesc = makeModuleDescriptor (Modules::stepSeq());
            const auto seqIds = rack::countableParamIds (seqDesc);
            expect (seqIds.joinIntoString (",") == "seqSync,seqRate,seqLength,seqGate,seqAccent" && allExist (seqIds),
                    "STEP SEQ (bodyOrder): the five globals in display order, pattern cells and spacers skipped");
            expect (rack::countableControls (seqDesc)[0].kind == CC::Kind::Combo
                    && rack::countableControls (seqDesc)[0].items == prm ("seqSync")->getNumSteps(),
                    "STEP SEQ: SYNC's item count is the parameter's");

            // Layer B: the frame's own ids.
            const auto filterB = rack::countableParamIds (makeModuleDescriptor (layerB (Modules::filter())));
            const auto seqB = rack::countableParamIds (makeModuleDescriptor (layerB (Modules::stepSeq())));
            expect (filterB.joinIntoString (",") == "bfilterType,bfilterCutoff,bfilterReso,bfilterTrack" && allExist (filterB)
                    && seqB.joinIntoString (",") == "bseqSync,bseqRate,bseqLength,bseqGate,bseqAccent" && allExist (seqB),
                    "layer B descriptors: the b ids, with and without bodyOrder");

            // A SYNTHETIC descriptor with every kind of element the editor adds after building
            // (indexIsValue / itemValues / noController combos, corner switches, alt rows, actions,
            // captions, displays, toggles, paged step knobs). The editor-built descriptors themselves
            // (SAMPLER, WAVETABLE, PERC, MOD MATRIX) are not covered by the selftest.
            rack::ModuleDescriptor d;
            {
                rack::Combo plain { "filterType", "TYPE", juce::StringArray { "a", "b" } };
                rack::Combo byIndex { "noiseType", "SET", juce::StringArray { "x", "y", "z" } };
                byIndex.indexIsValue = true;
                rack::Combo byValues { "noiseType", "KIT", juce::StringArray { "x", "y" } };
                byValues.itemValues = [] { return juce::Array<int> { 3, 5 }; };
                rack::Combo empty { "noiseType", "NONE", juce::StringArray {} };
                rack::Combo loader { "noiseType", "SET", juce::StringArray { "s1", "s2" } };
                loader.indexIsValue = true;
                loader.noController = true;
                rack::Combo dynamic { "noiseType", "DYN", std::function<juce::StringArray()> ([] { return juce::StringArray { "1", "2", "3", "4" }; }) };
                rack::Knob cell { "filterCutoff", "CUT" };
                cell.toggleParamId = "filterOn";
                cell.accentParamId = "filterOn";
                cell.altParamId = "filterReso";
                rack::Knob pattern { {}, "1" };
                pattern.patternStep = 0;
                rack::Knob paged { "seqPitch3", "3" };
                d.paging.pageCount = 4;
                d.paging.stepsPerPage = 48;
                d.paging.pagedPrefixes = { "seqPitch" };
                d.body = { rack::Caption { "x" }, plain, rack::Action { "GO", {}, {} }, byIndex, byValues, empty, cell, pattern, paged,
                           rack::Toggle { "filterOn", "ON" }, rack::Display { nullptr, 2 }, dynamic, rack::Knob { {}, "nothing" }, loader };
            }
            const auto mixed = rack::countableControls (d);
            expect (rack::countableParamIds (d).joinIntoString (",") == "filterType,noiseType,noiseType,filterCutoff,filterOn,noiseType",
                    "counted: combo, two list combos, knob (its main id only), toggle, provider combo; skipped: caption, action, noController combo, combo without items, pattern cell, paged step, display");
            expect (mixed.size() == 6 && mixed[4].kind == CC::Kind::Toggle && mixed[5].kind == CC::Kind::Combo && mixed[5].items == 4,
                    "a toggle is a toggle; a provider combo is asked for its items");
            expect (mixed.size() == 6 && mixed[1].kind == CC::Kind::List && mixed[1].listCount && mixed[1].listCount() == 3 && ! mixed[1].listValues
                    && mixed[2].kind == CC::Kind::List && mixed[2].listValues && mixed[2].listValues()[1] == 5,
                    "an indexIsValue / itemValues combo is a list control: it carries its item count and its value list");
            expect (mixed.size() == 6 && mixed[0].bodyIndex == 1 && mixed[1].bodyIndex == 3 && mixed[2].bodyIndex == 4 && mixed[3].bodyIndex == 6
                    && mixed[4].bodyIndex == 9 && mixed[5].bodyIndex == 11,
                    "each countable control names its body element (the frame puts the knob number on that cell)");
            expect (mixed.size() == 6 && mixed[0].bank == 0 && mixed[5].bank == 0, "no groups, six controls: one bank");
            expect (rack::countableParamIds (makeModuleDescriptor (Modules::filter())) == filterIds
                    && rack::countableParamIds (rack::ModuleDescriptor {}).isEmpty(),
                    "pure: the same descriptor gives the same list; an empty body has no countable control");

            // End to end with real descriptors, as the editor pushes them: FILTER (A) selected.
            auto frameOf = [] (const char* entry, bool isB, const rack::ModuleDescriptor& desc)
            {
                Frame f { entry, isB, {} };
                for (const auto& c : rack::countableControls (desc))
                    f.controls.push_back (CP::moduleControlFrom (c));
                return f;
            };
            run (mod, {});
            r.set (Parameters::ID::layerFocus, 0.0f);
            r.p->setControllerFrames ({ frameOf ("filter", false, filterDesc), frameOf ("stepseq", false, seqDesc) });
            expect (r.p->selectControllerModule ("filter", false) && sel().bankCount == 1, "FILTER's real frame: selectable, one bank");
            r.set ("filterType", 0.0f);
            cutoff->setValueNotifyingHost (0.3f);
            const float cutoffBefore = cutoff->getValue();
            run (mod, { Ev { 1, 30, 127 } });
            expect (raw ("filterType") == 1.0f && cutoff->getValue() == cutoffBefore && cutoffBefore < 1.0f,
                    "FILTER selected, knob 1 = 127: its 1st control (TYPE) at its last item, not the fallback");
            run (mod, { Ev { 1, 30, 0 } });
            expect (raw ("filterType") == 0.0f, "knob 1 = 0: TYPE back on its first item");
            r.p->setControllerFrames ({ frameOf ("filter", true, makeModuleDescriptor (layerB (Modules::filter()))), frameOf ("stepseq", false, seqDesc) });
            r.set ("bfilterType", 0.0f);
            run (mod, { Ev { 1, 30, 127 } });
            expect (sel().valid && sel().layerB && raw ("bfilterType") == 1.0f && raw ("filterType") == 0.0f,
                    "the rack now shows B's FILTER frame: still selected, knob 1 moves the b id");
            r.p->setControllerFrames ({ frameOf ("stepseq", false, seqDesc) });
            expect (! sel().valid, "FILTER no longer shown: selection dropped");
            r.set ("filterType", 0.0f);
            r.set ("bfilterType", 0.0f);

            // Slot banks and list combos: a descriptor of equal groups (the MOD MATRIX's slots) has
            // one bank per group; a list combo is stepped through its own value list.
            {
                rack::Combo modList { "noiseType", "MOD", juce::StringArray { "d", "b", "c", "a" } };
                modList.indexIsValue = true;
                modList.itemValues = [] { return juce::Array<int> { 3, 1, 2, 0 }; };
                int paramItems = 2;
                rack::Combo paramList { "filterType", "PARAM", std::function<juce::StringArray()> ([&paramItems]
                                        { juce::StringArray a; for (int i = 0; i < paramItems; ++i) a.add (juce::String (i)); return a; }) };
                paramList.indexIsValue = true;
                rack::ModuleDescriptor slots;
                slots.slotActivity.groupSize = 5;
                slots.body = { rack::Knob { "filterReso", "SRC" }, modList, paramList, rack::Knob { "attack", "QUANT" }, rack::Knob { "filterCutoff", "AMT" },
                               rack::Knob { "filterReso", "SRC" }, modList, paramList, rack::Knob { "attack", "QUANT" }, rack::Knob { "filterTrack", "AMT" } };
                const auto sc = rack::countableControls (slots);
                bool banksOk = sc.size() == 10;
                for (size_t i = 0; banksOk && i < sc.size(); ++i)
                    banksOk = sc[i].bank == (int) i / 5;
                expect (banksOk, "a descriptor of two groups of five: two banks, one per group (not sevens cutting through a slot)");
                rack::ModuleDescriptor wide;
                wide.slotActivity.groupSize = 9;
                for (int i = 0; i < 9; ++i) wide.body.push_back (rack::Knob { "filterCutoff", "K" });
                const auto wc = rack::countableControls (wide);
                expect (wc.size() == 9 && wc[6].bank == 0 && wc[7].bank == 1 && wc[8].bank == 1, "a group of more than seven is split into chunks of seven");
                const auto grainControls = rack::countableControls (makeModuleDescriptor (Modules::grain()));
                bool sevens = grainControls.size() > 7;
                for (size_t i = 0; i < grainControls.size(); ++i)
                    sevens = sevens && grainControls[i].bank == (int) i / 7;
                expect (sevens, "a module without groups (GRAIN's spec, more than seven controls): banks of seven");

                r.p->setControllerFrames ({ frameOf ("modmatrix", false, slots) });
                expect (r.p->selectControllerModule ("modmatrix", false) && sel().bank == 0 && sel().bankCount == 2, "slot banks: bank count = number of groups");
                r.set ("noiseType", 1.0f);
                run (mod, { Ev { 1, 31, 0 } });
                const float modFirst = raw ("noiseType");
                run (mod, { Ev { 1, 31, 127 } });
                expect (modFirst == 3.0f && raw ("noiseType") == 0.0f, "list combo with itemValues: the first / last item as shown, the parameter gets the item's own value (3 / 0)");
                forget();
                run (soft, { Ev { 1, 31, 100 } });
                const bool listStays = raw ("noiseType") == 0.0f;
                run (soft, { Ev { 1, 31, 68 } });
                expect (listStays && raw ("noiseType") == 2.0f,
                        "22.8, encoder on a list combo (4 items, on the last): a quarter of the travel left is the item shown before it, with its own value (2)");
                run (mod, {});
                r.set ("filterType", 0.0f);
                run (mod, { Ev { 1, 34, 127 } });
                const float paramLast = raw ("filterType");
                paramItems = 0;
                run (mod, { Ev { 1, 34, 0 } });
                expect (paramLast == 1.0f && raw ("filterType") == 1.0f, "list combo without itemValues: the index is the value; an empty list (asked when the value arrives) moves nothing");
                paramItems = 2;
                cutoff->setValueNotifyingHost (0.3f);
                prm ("filterTrack")->setValueNotifyingHost (0.0f);
                run (mod, { Ev { 1, 35, 127 } });
                const bool bank1Knob5 = cutoff->getValue() == 1.0f && prm ("filterTrack")->getValue() == 0.0f;
                nextBank();
                cutoff->setValueNotifyingHost (0.3f);
                run (mod, { Ev { 1, 35, 127 } });
                expect (bank1Knob5 && sel().bank == 1 && prm ("filterTrack")->getValue() == 1.0f && near (cutoff->getValue(), 0.3f),
                        "slot banks: knob 5 is AMT of slot 1, after one bank step AMT of slot 2 - the numbering restarts at 1 in every bank");
                r.set ("filterType", 0.0f);
                prm ("filterTrack")->setValueNotifyingHost (0.0f);
            }

            // "@power": a press on the button switches the selected module on or off.
            {
                auto on = [&] (const char* id) { return raw (id) > 0.5f; };
                auto filterA = frameOf ("filter", false, filterDesc);
                filterA.enableParam = filterDesc.enableParam;
                r.p->setControllerFrames ({ filterA, frameOf ("stepseq", false, seqDesc) });
                r.p->selectControllerModule ("filter", false);
                r.set ("filterOn", 0.0f);
                r.set ("bfilterOn", 0.0f);
                run (mod, { Ev { 1, 42, 127 } });
                const bool onAfterPress = on ("filterOn");
                run (mod, { Ev { 1, 42, 0 } });
                const bool onAfterRelease = on ("filterOn");
                run (mod, { Ev { 1, 42, 127 } });
                expect (filterDesc.enableParam == "filterOn" && onAfterPress && onAfterRelease && ! on ("filterOn"),
                        "@power: selected module off -> press -> on (the release changes nothing) -> press -> off");
                run (mod, { Ev { 1, 42, 127 }, Ev { 1, 42, 0 } });
                expect (on ("filterOn"), "@power: press and release inside one tick still toggle once");
                run (mod, { Ev { 1, 42, 127 }, Ev { 1, 42, 0 }, Ev { 1, 42, 127 }, Ev { 1, 42, 0 } });
                expect (on ("filterOn"), "@power: two presses inside one tick leave it as it was");
                r.p->clearControllerSelection();
                run (mod, { Ev { 1, 42, 127 } });
                expect (on ("filterOn"), "@power with nothing selected: nothing");
                r.p->selectControllerModule ("stepseq", false);   // pushed without an enable id
                run (mod, { Ev { 1, 42, 127 } });
                expect (on ("filterOn") && ! on ("seqOn"), "@power on a frame without an enable id: nothing");
                auto filterBFrame = frameOf ("filter", true, makeModuleDescriptor (layerB (Modules::filter())));
                filterBFrame.enableParam = "bfilterOn";
                r.p->setControllerFrames ({ filterBFrame });
                r.p->selectControllerModule ("filter", true);
                run (mod, { Ev { 1, 42, 127 } });
                expect (on ("bfilterOn") && on ("filterOn"), "@power on a B frame: the b id is switched, A's stays");
                r.set ("filterOn", 0.0f);
                r.set ("bfilterOn", 0.0f);
            }

            // An inactive (dimmed) control keeps its slot but is not moved.
            {
                bool cutoffActive = false;
                Frame f { "filter", false, { MC { "filterCutoff", CP::ControlKind::Knob, 0, [&cutoffActive] { return cutoffActive; } },
                                             MC { "filterReso", CP::ControlKind::Knob, 0 } } };
                r.p->setControllerFrames ({ f });
                r.p->selectControllerModule ("filter", false);
                cutoff->setValueNotifyingHost (0.3f);
                prm ("filterReso")->setValueNotifyingHost (0.0f);
                const float c0 = cutoff->getValue();
                run (mod, { Ev { 1, 30, 127 }, Ev { 1, 31, 127 } });
                expect (cutoff->getValue() == c0 && prm ("filterReso")->getValue() == 1.0f,
                        "inactive control: knob 1 moves nothing (not its fallback either), knob 2 still moves the 2nd control");
                cutoffActive = true;
                run (mod, { Ev { 1, 30, 127 } });
                expect (cutoff->getValue() == 1.0f, "active again: knob 1 moves it without a new push");
                rack::ModuleDescriptor gated;
                rack::Knob gk { "filterCutoff", "CUT" };
                gk.activeWhen = [] { return false; };
                gated.body = { gk, rack::Knob { "filterReso", "RESO" } };
                const auto gc = rack::countableControls (gated);
                expect (gc.size() == 2 && gc[0].active && ! gc[0].active() && ! gc[1].active
                        && CP::moduleControlFrom (gc[0]).active && ! CP::moduleControlFrom (gc[1]).active,
                        "a descriptor's activeWhen travels with the countable control into the pushed control");
            }
            r.p->setControllerFrames ({});
        }

        // Hosted (the Rig's processBlock is the DAW path): nothing is captured, Shift included.
        r.block ({ { 0, juce::MidiMessage::controllerEvent (1, 27, 127) }, { 1, juce::MidiMessage::controllerEvent (1, 29, 65) } });
        expect (! r.p->anyControllerCapturedForTest() && ! r.p->controllerShiftHeldForTest(), "hosted: Shift and encoder are not captured");
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
        controllerChecks();
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
