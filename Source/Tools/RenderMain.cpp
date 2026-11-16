// JASS_Render — headless offline renderer for null tests (Story 21.4, S0 of AD-16).
//
//   JASS_Render.exe <preset.jass> <out.wav> [seconds]
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
    std::vector<Ev> midiScript()
    {
        std::vector<Ev> e;
        auto note = [&e] (double on, double off, int n, float v) { e.push_back ({ on, n, v, true }); e.push_back ({ off, n, 0.0f, false }); };
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

    class Job : private juce::Timer
    {
    public:
        Job (juce::File presetIn, juce::File wavOut, double secondsIn)
            : preset (std::move (presetIn)), out (std::move (wavOut)), seconds (secondsIn)
        {
            juce::PluginHostType::jucePlugInClientCurrentWrapperType = juce::AudioProcessor::wrapperType_Undefined;
            proc = std::make_unique<SynthyProcessor>();
            proc->setRateAndBufferSizeDetails (kSampleRate, kBlock);
            proc->prepareToPlay (kSampleRate, kBlock);

            const auto res = PresetIO::loadFromFile (proc->getAPVTS(), preset);
            if (! res.ok) { fail ("cannot load preset " + preset.getFullPathName()); return; }
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
            const bool pending = h.pendingSamplerSetName.isNotEmpty() || h.pendingPercKitName.isNotEmpty();
            if (pending && juce::Time::getMillisecondCounterHiRes() - started < 180000.0)
                return;                              // sample sets still arriving
            stopTimer();
            if (pending)
                std::cerr << "JASS_Render: warning, sample set still pending after 180 s" << std::endl;
            render();
            juce::MessageManager::getInstance()->stopDispatchLoop();
        }

        void render()
        {
            const auto script = midiScript();
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
        double seconds;
        double started = 0.0;
        std::unique_ptr<SynthyProcessor> proc;
    };
}

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: JASS_Render <preset.jass> <out.wav> [seconds]" << std::endl;
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI juce;
    const double seconds = argc > 3 ? std::atof (argv[3]) : 10.0;
    int code = 0;
    {
        Job job (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[1])),
                 juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])),
                 seconds);
        if (job.exitCode == 0)
            juce::MessageManager::getInstance()->runDispatchLoop();
        code = job.exitCode;
    }
    return code;
}
