#pragma once
#include "ModuleSpec.h"

// UI-side aggregation: pulls every per-component header and lists all spec-driven modules. Used by
// the editor (to build rack descriptors) and by ModuleRegistry.cpp (to add APVTS params). Add a
// module here + its <Name>Specs.h include as it is migrated off the hand-written code.
#include "FilterSpecs.h"
#include "CompressorSpecs.h"
#include "StereoSpecs.h"
#include "MasterSpecs.h"
#include "SubSpecs.h"
#include "NoiseSpecs.h"
#include "FormantSpecs.h"
#include "DistortionSpecs.h"
#include "WavefoldSpecs.h"
#include "BitcrushSpecs.h"
#include "PhaserSpecs.h"
#include "ChorusSpecs.h"
#include "DelaySpecs.h"
#include "ReverbSpecs.h"
#include "ArpeggiatorSpecs.h"
#include "GlideSpecs.h"
#include "PitchEnvSpecs.h"
#include "OscSpecs.h"
#include "CrossModSpecs.h"
#include "LfoSpecs.h"
#include "ModMatrixSpecs.h"
#include "KarplusSpecs.h"
#include "WavetableSpecs.h"
#include "AdsrSpecs.h"
#include "DisplaySpecs.h"

namespace Modules
{
    inline std::vector<ModuleSpec> all()
    {
        return {
            filter(), compressor(), stereo(), master(), sub(), noise(),
            formant(), distortion(), wavefold(), bitcrush(), phaser(), chorus(),
            delay(), reverb(), arpeggiator(), glide(), pitchEnv(),
            osc(1), osc(2), osc(3), crossmod(), lfo(1), lfo(2), lfo(3), lfo(4), modMatrix(),
            string(), wavetable(), adsr(), oscilloscope(), spectrum(), keyboard(),
        };
    }
}
