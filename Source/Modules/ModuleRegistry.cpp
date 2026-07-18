#include "ModuleRegistry.h"
#include "AllModules.h"

// Bridges the audio-safe declaration (ModuleRegistry.h) to the UI-side module list (AllModules.h):
// only .params is read here, so pulling the UI headers into this one TU is harmless.
namespace Modules
{
    void appendAllParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& out)
    {
        for (const auto& m : all())
            appendModuleParameters (m.params, m.title, out);
    }
}
