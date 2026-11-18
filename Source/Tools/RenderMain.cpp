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

int main (int argc, char* argv[])
{
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
