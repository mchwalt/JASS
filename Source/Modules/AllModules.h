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

namespace Modules
{
    inline std::vector<ModuleSpec> all()
    {
        return {
            filter(), compressor(), stereo(), master(), sub(), noise(),
            formant(), distortion(), wavefold(), bitcrush(), phaser(), chorus(),
            delay(), reverb(), arpeggiator(), glide(), pitchEnv(),
        };
    }
}
