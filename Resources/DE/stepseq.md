Spielt eine Figur mit bis zu 192 Schritten, die du selbst schreibst, transponiert durch die gespielte Taste. **Die erste Taste startet sie, und sie läuft weiter** — du musst nichts gedrückt halten. Eine neue Taste setzt die Figur auf diesen Grundton, Hoch / Runter verschieben sie um eine Oktave, und die **Leertaste stoppt** sie. Das Ausschalten des Moduls ebenfalls.

- **1 … 48** — die Note des Schritts. Die Box zeigt die echte Tonhöhe (E1, C3 …), die der Schritt in der aktuellen Oktave der Klaviatur spielt — die Figur transponiert weiterhin mit der gespielten Taste. Der Schalter in der Ecke eines Knopfes schaltet den Schritt weiter: **an → akzentuiert (gefüllt: spielt härter, Filter öffnet) → aus (eine Pause)**.
- **GATE-Knopf (Kopfzeile)** — schaltet dieselben 48 Knöpfe auf die **Länge** jedes Schritts um: 5–100 % des Schritts, dann **TIE** (in den nächsten Schritt gehalten, der ohne neuen Anschlag übernimmt) und **SLIDE** (dasselbe, gleitend in die nächste Note — die 303). Zweiter Klick schaltet zurück auf PITCH.
- **SYNC** — Schrittlänge als Notenwert. Auf *Free* stellen, um stattdessen RATE zu benutzen.
- **RATE** — Schritte pro Sekunde, wenn SYNC auf *Free* steht.
- **LEN** — nach wie vielen Schritten das Muster wiederholt.
- **GATE** — Notenlänge, für das ganze Muster. **1 = Legato**: jeder Ton wird in den nächsten Schritt gehalten, ohne Lücke. Kleinere Werte kürzen alle Töne; die eigene Gate-Länge eines Schritts skaliert obendrauf.
- **ACCENT** — was ein akzentuierter Schritt tut: wie viel lauter er spielt und wie weit der Filter öffnet. Bei 0 ändern Akzente nichts.

Beim Klicken oder Drehen erklingt der Schritt, du kannst die Figur also nach Gehör schreiben. Eine **Pause behält ihre Note**: ihr Knopf ist gedimmt, dreht aber weiter — die Tonhöhe eines stummen Schritts lässt sich also einstellen und anhören, bevor er eingeschaltet wird. Ein **Doppelklick** setzt einen Knopf auf den Wert des geladenen Presets zurück. Vorschau, Eingabe und Notennamen folgen dem Grundton, auf dem die Figur klingt: der gelatchten Taste, solange eine läuft, sonst dem aktuellen C der Klaviatur (Hoch / Runter verschiebt es).

**Figur einspielen statt Knöpfe drehen.** Der **Reset**-Knopf (der Kreispfeil in der Kopfzeile dieses Moduls) leert das Muster und beginnt die Eingabe bei Schritt 1: ein Ring zeigt, welcher Schritt auf einen Ton wartet. Spiel eine Taste — Computertastatur oder MIDI-Keyboard — und sie wird dort eingetragen, eingeschaltet, und der Ring rückt weiter. **LEERTASTE** überspringt einen Schritt und lässt ihn als Pause. **← / →** verschieben den Ring, ohne etwas zu ändern, die **RÜCKTASTE** nimmt den letzten Ton zurück (ein Schritt zurück, ausgeschaltet), **ESC** beendet die Eingabe. Nach LEN Schritten endet die Eingabe von selbst. Ein **Klick auf einen Schrittknopf** setzt den Ring dorthin, ein falscher Ton wird also durch Anklicken und Neuspielen korrigiert. Solange der Ring zu sehen ist, schreiben die Tasten, statt das Muster zu starten.

Ersetzt den ARPEGGIATOR: es kann nur eines von beiden laufen, das Einschalten schaltet das andere aus.

**MIDI-Import/-Export — die Knöpfe LOAD MIDI / SAVE MIDI in der Titelzeile dieses Moduls.** LOAD MIDI öffnet eine `.mid`-Transkription: Velocity wird zu Akzenten, Notenlängen zu Gate/TIE/SLIDE, der Zyklus des Loops wird erkannt, und die Figur latcht auf ihren häufigsten Ton und spielt. SAVE MIDI schreibt die Figur wieder heraus. Eine Asymmetrie: ein TIE mit Tonhöhenwechsel wird als 303-Überlappung exportiert und kommt als SLIDE zurück — MIDI kann den Unterschied nicht transportieren.

Eine **rote Linie** hinter einem Schritt markiert das Pattern-Ende (LEN): alles rechts davon bleibt erhalten, spielt aber nicht.

**Seiten (< 1/4 >, Kopfzeile).** Die 48 Knöpfe zeigen eine Seite des Musters; **< / >** blättern durch vier Seiten à 48 (LEN bis 192 = 12 Takte Sechzehntel). Die Anzeige nennt Seite / Seiten, ein Punkt zeigt, wo das Muster gerade spielt. Mit gerastetem **FOLLOW** blättert die Anzeige mit dem Muster mit; von Hand blättern pausiert FOLLOW — erneut rasten springt zurück und folgt wieder. Die Eingabe mit dem Ring wechselt die Seite von selbst.
