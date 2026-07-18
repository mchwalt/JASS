# JASS — Modul-Architektur: „ein Modul = ein Ort" (Konzept)

Status: **Entwurf zur Entscheidung** (2026-07-18). Kein Code geändert. Zweck: prüfen, ob wir
die verstreute Modul-Definition auf **eine deklarative Spezifikation pro Modul** ziehen, aus der
Parameter (APVTS), Persistenz (`.synthy`) und Rack-UI **generiert** werden.

---

## 1. Ausgangslage (Ist)

Ein „Modul" (z. B. FILTER) ist heute über **vier Dateien** verteilt — Beispiel FILTER:

| Aspekt | Datei / Stelle | Code (gekürzt) |
|---|---|---|
| Parameter-IDs | `Parameters.h` (ID-Namespace) | `filterOn`, `filterType`, `filterCutoff`, `filterReso` |
| Parameter-Anlage (APVTS) | `Parameters.h::createLayout` | 4× `params.push_back(AudioParameter…)` |
| DSP-Verrohrung | `Parameters.h::applyToVoice` | `filter.setType(...); filter.setCutoff(...)` |
| Persistenz schreiben | `PresetIO.h::toVar` | `root->setProperty("FilterCutoff", …)` |
| Persistenz lesen | `PresetIO.h::applyVar` | `setRaw(a, ID::filterCutoff, jnum(v,"FilterCutoff",…))` |
| Rack-UI | `PluginEditor.cpp` | `add(Zone::Processing, W4H1, Processor, "FILTER", filterOn, { … })` |
| Hilfe | `Resources/{EN,DE}/filter.md` | Text |

**Folgen:**
- Einen Parameter hinzufügen = **4–5 Stellen** anfassen (steht so in `project-context.md`). Fehlt eine → halb verdrahteter Param.
- Die `.synthy` ist **flach** („Kraut und Rüben"): `"FilterType"`, `"FilterCutoff"`, `"ReverbRoomSize"` … nebeneinander. Nur die OSCis sind von Hand als verschachteltes Array geschrieben.
- Die Rack-UI IST bereits deklarativ (`ModuleDescriptor`, AD-1) — aber sie **referenziert** nur Parameter-IDs, sie **besitzt** die Parameter nicht.

---

## 2. Ziel & Prinzip

**Ein Modul wird an genau EINER Stelle deklariert** — einer `ModuleSpec` (am besten eine Header-
Datei pro Modul, z. B. `Source/Modules/FilterModule.h`). Aus der Deklaration werden **generiert**:

- das **APVTS-Layout** (statt `createLayout` von Hand),
- die **`.synthy`-Persistenz**, automatisch **verschachtelt pro Modul**,
- der **Rack-UI-Descriptor** (Körper aus den Parametern).

**Handgeschrieben bleibt** (bewusst — kann kein Header generieren):
- die eigentliche **DSP-`process()`-Logik** jedes Moduls (jedes rechnet anders),
- die **Voice-Verrohrung** (welches Param welchen DSP-Setter treibt) — bleibt, liest aber die IDs aus der Spec.

> Das ist **kein** Neubau — es ist die **Vollendung von AD-1**: die UI ist schon deklarativ, wir
> ziehen Parameter + Persistenz auf dasselbe Modell.

---

## 3. Datenmodell

```cpp
// Ein Parameter — genug Info, um APVTS + Persistenz + UI daraus zu bauen.
struct ParamSpec
{
    juce::String id;            // APVTS-ID, z. B. "filterCutoff"
    juce::String persistKey;    // .synthy-Schlüssel IM Modul-Objekt, z. B. "Cutoff"
    juce::String uiLabel;       // Knopf-Beschriftung, z. B. "CUTOFF"
    juce::String legacyKey;     // alter FLACHER .synthy-Name für Migration, z. B. "FilterCutoff"

    enum class Kind { Float, Int, Bool, Choice };
    Kind kind;
    juce::NormalisableRange<float> range;   // Float/Int
    float defaultValue;
    juce::StringArray choices;              // Choice
    rack::ModTarget modTarget = rack::ModTarget::None;   // optional: Mod-Ring
    // optional: Display-Transform (FREQ = base×ratio) als Funktionspaar
};

// Ein Modul.
struct ModuleSpec
{
    juce::String id;               // "filter"  (== Hilfe-id, Layout-Key)
    juce::String title;            // "FILTER"
    juce::String persistObject;    // .synthy-Objekt-Key, z. B. "Filter"
    rack::ModuleType type;         // Processor
    rack::Zone       zone;         // Processing
    rack::SizeClass  size;         // W4H1
    bool defaultVisible = true;
    juce::String enableParamId;    // "" oder die ID eines Bool-Params (Header-Toggle)

    std::vector<ParamSpec> params;

    // Nicht-Parameter-Körper (Action-Buttons, Displays) + abgeleiteter Enable-Zustand:
    std::function<bool()>                 enabledWhen;   // z. B. CROSS MOD (osc1&&osc2)
    std::vector<rack::BodyElement>        extraBody;     // PLUCK-Button, ADSR-Kurve, Scope …
};
```

Eine **Registry** (`std::vector<ModuleSpec>`, in einer Datei aufgesammelt, oder per Selbst-
Registrierung je Header) ist die neue zentrale Liste — das „Master-File", nach dem du gefragt hast.

---

## 4. Was daraus generiert wird

```cpp
// (a) APVTS — ersetzt createLayout():
for (auto& m : registry)
  for (auto& p : m.params)
     layout.add( makeAudioParameter(p) );      // Kind → AudioParameterFloat/Choice/Bool/Int

// (b) Rack-UI — ersetzt die add(...)-Aufrufe:
for (auto& m : registry)
   rack.addModule( makeDescriptor(m) );          // params → Knob/Combo-Body + type/zone/size/visible/enable + extraBody

// (c) Persistenz — VERSCHACHTELT, ersetzt toVar/applyVar:
for (auto& m : registry) {
   auto* obj = new DynamicObject();
   for (auto& p : m.params) obj->setProperty(p.persistKey, read(p));
   root->setProperty(m.persistObject, obj);      // => "Filter": { "Cutoff": …, … }
}
```

Die DSP-Verrohrung (`applyToVoice`) bleibt, referenziert die IDs aber aus der Spec (`m.params[i].id`).

---

## 5. Ziel-`.synthy` (vorher → nachher)

**Vorher (flach):**
```json
"FilterType": "Lowpass", "FilterCutoff": 500.0, "FilterResonance": 2.5,
"ReverbEnabled": true, "ReverbRoomSize": 0.5, "ReverbDamping": 0.4, "ReverbMix": 0.28,
"Lfo1Waveform": "Sine", "Lfo1Target": "FilterCutoff", "Lfo1Rate": 3.0, ...
```

**Nachher (pro Modul verschachtelt, wie die OSCis):**
```json
"Filter":  { "Enabled": true, "Type": "Lowpass", "Cutoff": 500.0, "Resonance": 2.5 },
"Reverb":  { "Enabled": true, "RoomSize": 0.5, "Damping": 0.4, "Mix": 0.28 },
"Lfos":    [ { "Enabled": true, "Wave": "Sine", "Target": "FilterCutoff", "Rate": 3.0, "Depth": 0.5 },
             { "Enabled": false, ... } ],
"ModMatrix": { "Enabled": true,
               "Slots": [ { "Source": "Envelope", "Target": "Cutoff", "Amount": 0.5 }, ... ] },
"Oscillators": [ { ... }, { ... }, { ... } ]
```

Nebeneffekt-Bonus: Der `choiceOrOff`-Trick (Bypass in „Off" des Choice gefaltet, nur für C# nötig)
kann **weg** — Enable wird ein sauberer eigener `"Enabled"`-Bool im Modul-Objekt.

---

## 6. Migration (alte Presets nicht brechen)

`applyVar` liest **nested-first, flach als Fallback** — pro Parameter über `legacyKey`:
```cpp
juce::var moduleObj = v[m.persistObject];                       // neu (nested)
for (auto& p : m.params) {
   juce::var val = moduleObj.isObject() ? moduleObj[p.persistKey] : juce::var();
   if (val.isVoid()) val = v[p.legacyKey];                       // alt (flach)
   if (! val.isVoid()) apply(p, val);                            // sonst: Werks-Default (Factory-Reset vorab)
}
```
`kFormatVersion` **2 → 3**. Deine bestehenden Presets (Default, Helikopter, MyPreset, whuwhu, LiveState)
laden weiter korrekt; beim nächsten Speichern werden sie im neuen Format geschrieben.

---

## 7. Sonderfälle (ehrlich)

| Fall | Modul(e) | Lösung |
|---|---|---|
| Enable + Choice gefaltet | Filter/Distortion/Noise/LFO | Enable wird eigener Bool-Param; „Off"-Faltung entfällt (C# weg) |
| Abgeleiteter Enable | CROSS MOD (`osc1&&osc2`) | `enabledWhen`-Lambda in der Spec (optional) |
| Action-/File-/Display-Körper | STRING (PLUCK), WAVETABLE (LOAD WAV), ADSR-Kurve, Scope/Spectrum | `extraBody` (Liste vorhandener `BodyElement`s) + optionale Lambdas — NICHT rein-datengetrieben |
| Indizierte Module | OSC 1–3, LFO 1–2 | Spec-**Fabrik** `makeOscSpec(i)` in Schleife (wie heute `addOsc`); Persistenz als Array |
| Wiederholte Sub-Objekte | ModMatrix-Slots | Spec unterstützt „Array-of-Sub-Params" (wie OSC-Array) |
| Tempo-Sync/Sonderlogik | Delay/LFO Rate | bleibt im DSP-Code (Spec liefert nur die Params) |

→ Die Spec ist **Daten + wenige optionale Lambdas**, keine 100 %-Magie. ~80 % der Module sind rein
datengetrieben, der Rest trägt 1–2 Hooks.

---

## 8. Inkrementeller Rollout (kein Big-Bang)

Registry und die drei Generatoren werden eingeführt, dann **Modul für Modul** migriert:
- `createLayout()` = generierte Specs **+** noch nicht migrierte Hand-Params (koexistieren).
- `toVar/applyVar` = Spec-Schleife **+** Hand-Rest.
- Nach jedem migrierten Modul: Build + Ohr-Test, Default byte-identisch.
- Am Ende: Hand-Code ist leer, alles läuft über Specs.

Reihenfolge-Vorschlag: erst ein einfaches Processor-Modul (FILTER) als Proof, dann die übrigen
Effekte, dann Generatoren/indizierte, zuletzt die Sonderfälle (ModMatrix, Displays).

---

## 9. Proof: FILTER als `ModuleSpec` (ersetzt alle 4 Stellen)

```cpp
inline ModuleSpec filterModule()
{
    ModuleSpec m;
    m.id = "filter"; m.title = "FILTER"; m.persistObject = "Filter";
    m.type = ModuleType::Processor; m.zone = Zone::Processing; m.size = SizeClass::W4H1;
    m.defaultVisible = true; m.enableParamId = "filterOn";
    m.params = {
        { "filterOn",     "Enabled",   "",       "",                Kind::Bool,   {}, 0.0f },          // Header-Toggle
        { "filterType",   "Type",      "TYPE",   "",                Kind::Choice, {}, 0.0f, {"Lowpass","Highpass"} },
        { "filterCutoff", "Cutoff",    "CUTOFF", "FilterCutoff",    Kind::Float,  {20.f,20000.f,1.f,0.3f}, 550.f, {}, ModTarget::FilterCutoff },
        { "filterReso",   "Resonance", "RESO",   "FilterResonance", Kind::Float,  {0.1f,10.f,0.01f},       0.707f, {}, ModTarget::FilterResonance },
    };
    return m;
}
```

Das ersetzt: die 4 IDs, die 4 `createLayout`-Zeilen, die 3 `toVar`- + 3 `applyVar`-Zeilen und den
`add("FILTER", …)`-Aufruf. Übrig bleibt im DSP nur `filter.setType/​setCutoff/​setResonance` (liest die IDs).

---

## 10. Aufwand & Risiko

- **Aufwand:** mehrschrittig (mehrere Sitzungen). Grob: 1× Infrastruktur (Registry + 3 Generatoren + Migration), dann ~25 Module migrieren (die meisten trivial, ~5 mit Hooks).
- **Risiko:** mittel. Absicherung = inkrementell + „Default byte-identisch" nach jedem Modul + die bestehende `.synthy`-Migration. APVTS-IDs bleiben gleich (nur Persistenz-Struktur ändert sich) → DAW-State bleibt heil.
- **Gewinn:** ein Modul = ein Ort; neuer Parameter = 1 Zeile in 1 Datei; `.synthy` sauber verschachtelt; neues Modul = 1 Header + DSP-Klasse.

---

## 11. Entscheidungen (getroffen 2026-07-18/19)

1. ✅ **Zentrale Specs-Datei zuerst** (`Source/ModuleSpecs.h`), später in Header-pro-Modul splittbar.
2. ✅ **DSP getrennt** lassen (`Source/DSP/`) — die Spec beschreibt nur Params/UI/Persistenz.
3. ✅ **Kein dauerhafter Migrations-Fallback** — die wenigen vorhandenen Presets werden **einmalig konvertiert** (flach → nested), dann liest/schreibt PresetIO nur noch das nested-Format. (Passiert erst im koordinierten Persistenz-Schritt.)

## 13. Fortschritt (2026-07-19)

- ✅ **Layer-Trennung + Registry** (`Source/Modules/`): `ParamSpec.h` (audio) / `ModuleSpec.h` (UI) / `ModuleRegistry.{h,cpp}` / `AllModules.h`. `Parameters.h` ist UI-frei.
- ✅ **17 reine Parameter-Module** spec-getrieben, je eigener `<Name>Specs.h`: Filter, Compressor, Stereo, Master, Sub, Noise, Formant, Distortion, Wavefold, Bitcrush, Phaser, Chorus, Delay, Reverb, Arpeggiator, Glide, PitchEnv. **Param-Anzahl exakt erhalten** (byte-identisch). Model-Erweiterungen: `showInBody` (SUB-Octave), `displayChoices` (DISTORTION-Anzeige), `freqDisplay` (für OSC vorbereitet).
- ✅ **ALLE 30 Module spec-getrieben** (Parameter). `createLayout()` = nur noch `Modules::appendAllParameters()`.
  - *Voll spec-bar (Factory):* OSC 1–3 (freqDisplay/AMP-Ring), LFO 1–2, MOD MATRIX (Slot-Zeilen).
  - *Params spec-bar, UI editor-gebunden (Display/Action/enabledWhen):* CROSS MOD (enabledWhen injiziert), STRING-KARPLUS (PLUCK), WAVETABLE (LOAD WAV), ENVELOPE-ADSR (Kurve), OSCILLOSCOPE, SPECTRUM, KEYBOARD.
- ✅ **`.synthy` nested pro Modul (v3)** — `PresetIO::toVar/applyVar` generieren aus den Specs (`Modules::writeState/readState`): ein Objekt je Modul (persistObject) mit Feldern (persistKey), Choice als kanonischer String, kein `choiceOrOff`-„Off" mehr. **Einmal-Konvertierung** beim Start (Standalone): alte flache Presets (v<3) + LiveState → nested; Originale nach `%AppData%\Synthy\PresetsBackup_v2` gesichert. `kFormatVersion` 2→3.

**Damit ist das Konzept vollständig umgesetzt.** Verbleibende Feinheiten: repo `DemoPresets/*.synthy` sind noch flach (werden on-device konvertiert); ~7 Module haben editor-gebundene UI (unvermeidbar); Layer-Trennung sauber (`Parameters.h`/`PresetIO.h` nutzen nur die audio-safe Registry).

## 12. Stand (Proof umgesetzt, 2026-07-19)

**FILTER läuft spec-getrieben** — `Source/ModuleSpec.h` (Modell + Generatoren `makeParameter`/
`appendModuleParameters`/`makeModuleDescriptor`) + `Source/ModuleSpecs.h` (`Modules::filter()`).
`createLayout` erzeugt die Filter-Params aus der Spec; der Rack-Descriptor wird generiert. **APVTS
+ UI aus EINER Deklaration.** Persistenz + DSP unverändert (Default byte-identisch, alte Presets
laden). Nächste Schritte: übrige Module Zug um Zug in Specs überführen; zuletzt Persistenz auf
nested + Einmal-Konvertierung. **Layering-Fund:** `ModuleSpec.h` zieht die UI-`ModuleDescriptor.h`
in die Audio-Schicht (via Parameters.h) — bei Fortführung trennen (Param-Gen audio, Descriptor-Gen UI).
