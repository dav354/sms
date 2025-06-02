**Doku: Das parallele 8080-Busprotokoll für Displays**

Das parallele 8080-Busprotokoll, auch bekannt als „Intel 8080 Interface“ oder „i80“, ist ein schnelles Kommunikationsinterface, das ursprünglich für den Intel 8080 Mikroprozessor entwickelt wurde. Wegen seiner Einfachheit und hohen Übertragungsrate ist es heute ein beliebter Standard für die Ansteuerung von LCD-Controllern wie ILI9341, ST7735 oder ST7789, besonders wenn serielle Schnittstellen (SPI, I²C) zu langsam sind.

**Kernkonzept**

Beim 8080-Bus werden 8 oder 16 Datenbits parallel über separate Leitungen übertragen – das heißt, pro Taktzyklus können 1 oder 2 Byte verschickt werden.

**Wichtige Signalleitungen**

* **Datenbus (D0-D7):** Überträgt die eigentlichen Daten (Pixel oder Befehle).
* **WR (Write Strobe):** Signalisiert (meist „active-low“), dass der Mikrocontroller gültige Daten bereitstellt, die vom Display übernommen werden sollen.
* **RD (Read Strobe):** Zeigt an, dass der Mikrocontroller Daten vom Display lesen möchte. Oft ungenutzt, wenn nur geschrieben wird.
* **D/C (Data/Command, auch RS):** Unterscheidet zwischen Befehlen (D/C=Low) und Daten (D/C=High) auf dem Bus.
* **CS (Chip Select):** Wählt das Display-Modul aus (active-low). Ist CS inaktiv, ignoriert das Display alle anderen Signale.
* **RST (Reset):** Setzt den LCD-Controller zurück, meist nur bei der Initialisierung benötigt.

**Typischer Schreibzyklus**

1. **CS aktivieren:** Display auswählen (CS auf aktiv, meist Low).
2. **D/C setzen:**

   * Low für Befehl
   * High für Daten
3. **Daten anlegen:** 8 Bits auf die Datenleitungen legen.
4. **WR-Puls:** Einen kurzen Low-Puls auf WR senden – dabei übernimmt das Display die Daten.
5. **(Optional) Weitere Daten senden:** Je nach Befehl.
6. **CS deaktivieren:** Display abwählen.

**Beispiel: Senden eines Befehls mit Daten**

Angenommen, es soll der Befehl „Spaltenadresse setzen“ (0x2A) gefolgt von vier Datenbytes gesendet werden:

* CS aktivieren
* D/C auf Low
* 0x2A auf Datenbus, WR-Puls
* D/C auf High
* Viermal: Datenbyte auf Datenbus, WR-Puls
* CS deaktivieren

**Pixeldaten übertragen**

Nach der Initialisierung (z. B. Fenster setzen, Farbmodus wählen) wird meist ein Befehl wie „Speicher schreiben“ (z. B. 0x2C) gesendet. Dann bleibt D/C auf High, und es folgen fortlaufend Pixeldaten, jeweils bestätigt durch WR-Pulse.

**Vorteile**

* **Hohe Geschwindigkeit:** Ideal für hochauflösende, farbige Displays mit vielen zu übertragenden Daten.
* **Einfache Logik:** Kein komplexes Protokoll, einfach zu implementieren.

**Nachteile**

* **Viele Pins nötig:** 8–16 für Daten + 3–5 für Steuerung – kann bei kleinen Mikrocontrollern zum Problem werden.
* **Komplexeres Leiterplattenlayout:** Mehr Leitungen bedeuten aufwendigere Platinen.

**Timing**

Die genauen Zeitvorgaben (Setup, Hold, Pulsbreiten) stehen im Datenblatt des jeweiligen Controllers und müssen eingehalten werden. Viele moderne Mikrocontroller oder Bibliotheken (z. B. `esp_lcd_panel_io_i80` im ESP-IDF) übernehmen die Timingeinhaltung automatisch.

**Fazit**

Das 8080-Protokoll bietet eine robuste, schnelle und leicht verständliche Möglichkeit, Displays parallel anzusteuern. Es ist ideal, wenn Geschwindigkeit wichtiger ist als eine minimale Pinanzahl – die Grundlage vieler moderner Display-Shields und -Module.
