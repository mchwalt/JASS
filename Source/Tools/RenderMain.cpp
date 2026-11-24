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
            if (p.name == "MiniLab 3") expect (p.controls.size() == 12, "MiniLab 3: all 12 controls known");
            if (p.name == CP::kGenericName) expect (p.controls.size() == 6, "Generic (GM): all 6 controls known");
        }

        CP::Profile test;
        const bool parsed = CP::parseText (R"({ "name": "Test", "match": "Test Box", "controls": [
              { "name": "ok",     "cc": 74, "channel": 0, "param": "filterCutoff" },
              { "name": "nope",   "cc": 75, "channel": 0, "param": "nope" },
              { "name": "badcc",  "cc": 300, "param": "filterReso" },
              { "name": "nocc",   "param": "filterReso" },
              { "name": "hugecc", "cc": 1e300, "param": "filterReso" },
              { "name": "strch",  "cc": 77, "channel": "2", "param": "filterReso" },
              { "name": "resch",  "cc": 78, "channel": 15, "param": "filterReso" },
              { "name": "badpin", "cc": 76, "param": "filterReso", "layer": "C" },
              { "name": "pinB",   "cc": 20, "channel": 2, "param": "filterCutoff", "layer": "B" },
              { "name": "pinA",   "cc": 21, "param": "filterCutoff", "layer": "a" },
              { "name": "tempo",  "cc": 22, "param": "syncTempo" },
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
