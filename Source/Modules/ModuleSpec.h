#pragma once
#include "ParamSpec.h"
#include "../UI/rack/ModuleDescriptor.h"
#include <functional>

// UI half of the module-spec system: the full ModuleSpec (params + UI placement + optional hooks)
// and the rack-descriptor generator. Pulls the rack headers, so this is included by the UI layer
// and by the per-module <Name>Specs.h headers — NOT by Parameters.h (which uses ParamSpec.h only).

struct ModuleSpec
{
    juce::String id;             // stable slug ("filter") — help id + layout key
    juce::String helpId;         // online-help slug; empty => id. Instanced modules share one text
                                 // (LFO 1..4 => "lfo", OSC 1..3 => "osc1") instead of duplicating help.
    juce::String title;          // display title ("FILTER")
    juce::String persistObject;  // .synthy object key ("Filter") — for the future nested format
    juce::String enableParamId;  // id of the Bool param that is the header on/off ("" => always on)
    rack::ModuleType type {};
    rack::Zone       zone {};
    rack::SizeClass  size {};
    bool defaultVisible = true;
    bool alignRight = false;     // pack right within the zone row (MASTER BUS: STEREO/MASTER/COMPRESSOR)
    std::vector<ParamSpec> params;

    // Optional hooks for the few non-pure modules (CROSS MOD, STRING, WAVETABLE, displays):
    std::function<bool()>          enabledWhen;   // derived lit/dim state (reads APVTS only)
    std::function<void()>          onReset;       // extra non-param reset (e.g. a display's time-base)
    std::vector<rack::BodyElement> extraBody;     // appended AFTER the param-derived body (actions, displays)

    // Optional DISPLAY order (16.2): when non-empty, the body is built from THESE param ids, in
    // THIS order — decoupling what the user sees from the params vector, whose order is the
    // append-only registration contract. Grew out of the 48-step STEP SEQ: steps 33..48 register
    // at the END (old DAW state keeps its indices) but must DISPLAY between step 32 and the
    // global knobs. An empty string is a spacer cell (an empty Caption) — one blank cell between
    // the figure and the globals keeps the rows aligned and the border readable. Ids listed here
    // must exist and be showInBody; anything not listed gets no body cell.
    std::vector<juce::String> bodyOrder;
};

// Build the rack UI descriptor from a spec: the enable Bool becomes the header toggle; every other
// param becomes a body element (Choice=>Combo, Bool=>Toggle, Float/Int=>Knob + optional ring /
// FREQ-transform); extraBody is appended last.
inline rack::ModuleDescriptor makeModuleDescriptor (const ModuleSpec& m)
{
    rack::ModuleDescriptor d;
    d.sizeClass = m.size; d.type = m.type; d.defaultZone = m.zone;
    d.defaultVisible = m.defaultVisible; d.id = m.id; d.helpId = m.helpId; d.title = m.title;
    d.enableParam = m.enableParamId;
    d.alignRight = m.alignRight;
    d.enabledWhen = m.enabledWhen;
    d.onReset = m.onReset;

    auto emit = [&d, &m] (const ParamSpec& p)
    {
        if (p.id == m.enableParamId || ! p.showInBody) return;   // enable bool / hidden params: no body cell
        if (p.kind == ParamSpec::Kind::Choice)
            d.body.push_back (rack::Combo { p.id, p.uiLabel, p.displayChoices.isEmpty() ? p.choices : p.displayChoices });
        else if (p.kind == ParamSpec::Kind::Bool)
            d.body.push_back (rack::Toggle { p.id, p.uiLabel });
        else
        {
            rack::Knob k { p.id, p.uiLabel };
            k.modTarget = p.modTarget;   // same type now (rack::ModTarget is an alias for LFOTarget)
            if (p.freqDisplay)
            {
                k.toDisplay   = [] (double base,  double ratio) { return base  * ratio; };
                k.fromDisplay = [] (double shown, double ratio) { return shown / ratio; };
            }
            d.body.push_back (k);
        }
    };
    if (m.bodyOrder.empty())
    {
        for (const auto& p : m.params)
            emit (p);
    }
    else
    {
        // Explicit display order (16.2): the params vector stays the registration contract,
        // this list is what the eye gets. "" = a spacer cell.
        for (const auto& id : m.bodyOrder)
        {
            if (id.isEmpty()) { d.body.push_back (rack::Caption { {} }); continue; }
            for (const auto& p : m.params)
                if (p.id == id) { emit (p); break; }
        }
    }
    for (const auto& e : m.extraBody)
        d.body.push_back (e);
    return d;
}
