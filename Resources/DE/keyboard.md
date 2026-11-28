Die Bildschirm-Klaviatur zum Spielen von Noten per Maus oder Computer-Tastatur.

**88 Tasten, A0–C8** — der volle Klavierumfang, gleiches Schema wie ein Flügel oder ein 88er-Digitalpiano. MIDI-Eingang ist davon nicht begrenzt: Noten außerhalb des sichtbaren Bereichs klingen trotzdem.

Blende dieses Modul aus (über das **MODULES**-Menü oder das Customization-Panel), wenn du über eine externe MIDI-Tastatur spielst — es beeinflusst nie den Klang, nur die Anzeige.

**Computer-Tastatur**

Gespielt wird über die volle Breite der Buchstabentasten: die **Grundreihe** (`a s d f g h j k l ö ä #`) sind die weißen Tasten, die **obere Reihe** darüber (`w e t z u o p +`) die schwarzen — rund 2½ Oktaven.

- **↑ / ↓ Pfeiltaste** — verschiebt den gesamten Bereich eine Oktave hoch / runter
- **Leertaste** — Karplus-String erneut anzupfen

Die Klaviatur muss den Fokus haben, damit die Buchstabentasten klingen (holt ihn beim Start automatisch; einmal draufklicken, falls die Tasten nicht mehr reagieren).

**Akkorde**

Normale Tastaturen verschlucken manche Dreier-Kombinationen — der dritte Ton kommt nie an. Oktave verschieben oder über MIDI spielen; die Buchstabentasten senden zudem eine feste Anschlagstärke.

**MIDI-Controller (Drehregler und Fader)**

Drehregler und Fader eines MIDI-Controllers bewegen JASS in der Standalone-App (in einer DAW dort zuweisen).

- **Header-Auswahl** — **Auto** wählt das Profil nach dem Namen des MIDI-Eingangs (MiniLab 3, sonst Generic (GM)); oder von Hand wählen
- **MiniLab 3 Regler 1–7** — die Regler des markierten Moduls, von links nach rechts: Drehregler, Combos (Drehen schaltet ihre Einträge durch) und Schalter (deutlich nach rechts ist an, nach links aus); übersprungen: Step-Zellen und die Combos SET, BANK und KIT
- **Nichts markiert** — Regler 1–7: Cutoff, Resonance, LFO 1 Rate, LFO 1 Depth, Chorus, Delay, Reverb
- **Modul markieren** — mit dem MiniLab-3-Profil: auf die Titelleiste oder einen seiner Regler klicken, oder Regler 8 nach rechts / links drehen für das nächste / vorige Modul; das markierte Modul hat einen weißen Rahmen, seine Regler tragen die Nummer ihres Drehreglers; ein ausgegrauter Regler hat keine und wird nicht bewegt
- **An / aus** — Hauptencoder drücken: schaltet das markierte Modul an oder aus
- **Markierung aufheben** — noch einmal auf die Titelleiste des markierten Moduls klicken; ein Preset laden oder RESET hebt sie ebenfalls auf
- **Bänke** — ein Modul mit mehr als 7 Reglern: Shift halten und Regler 8 nach rechts / links drehen für die nächste / vorige Bank; die Titelleiste zeigt sie, z. B. 2/3; MOD MATRIX: eine Bank pro Slot
- **Programme** — gleich im ARTURIA- und im DAW-Programm (Shift + Pad 3)
- **MiniLab 3 Fader** — MASTER VOL, TEMPO, A LVL, B LVL, egal welchen Layer du bearbeitest
- **Tempo fein** — Shift + Hauptencoder bewegt TEMPO um 1 BPM pro Raste
- **Generic (GM)** — CC 74 Cutoff, 71 Resonance, 73 Attack, 72 Release, 91 Reverb, 93 Chorus
- **Keine Sprünge** — ein Drehregler bewegt den Wert von dort, wo er steht, um so viel, wie du drehst; ein Fader bewegt ihn in seine Richtung, und beide kommen oben und unten gemeinsam an
- **Drehregler am Anschlag** — weiterdrehen: der Wert läuft weiter
- **Layer** — ein Regler bewegt den Layer, den du bearbeitest: A, B oder beide bei A+B
- **Dateien** — eine `.json` pro Controller in `%AppData%\JASS\Controllers`; dort `cc` und `param` ändern (MiniLab 3 Regler 1–7: `fallback`), Datei löschen holt beim nächsten Start die Werksfassung zurück
- **`"takeover"`** — pro Regler in der Datei: `"encoder"` für einen Endlos-Drehregler, `"scale"` für einen Fader (Standard), `"jump"` setzt die Stellung sofort; eine `MiniLab 3`-Datei von vor dieser Version hat keins, ihre Drehregler verhalten sich wie Fader, bis du sie löschst
