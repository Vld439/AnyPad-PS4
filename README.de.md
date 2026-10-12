<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS5"></p>

# AnyPad PS5: Befreie deine PS5 vom DualSense

**Eine Bluetooth-Controller-Brücke für PS5-Konsolen mit Jailbreak.**

[English · Español](README.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [Português](README.pt.md) · **Deutsch** · [Русский](README.ru.md) · [简体中文](README.zh.md) · [繁體中文](README.zh-Hant.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md)

---

Mit AnyPad PS5 kannst du Controller anderer Plattformen an deiner PS5 verwenden. Es verbindet sie über den Bluetooth-Chip der Konsole und stellt sie dem System als virtuelle DualSense-Controller dar.

Kein USB-Adapter nötig.

**Version 0.9.1-beta.** Getestet auf einer PS5 fat, Firmware 10.01, mit einem DualShock 4 und einem 8BitDo SF30 Pro (Switch-Modus), in den Konsolenmenüs, in Spielen und in Emulatoren.

**In zwölf Sprachen verfügbar:** Englisch, Spanisch, Französisch, Italienisch, Portugiesisch, Deutsch, Russisch, vereinfachtes Chinesisch, traditionelles Chinesisch, Japanisch, Koreanisch und Arabisch. Die AnyPad-Seite folgt der Sprache der Konsole.

---

## Über dieses Projekt

AnyPad PS5 begann am 3. Oktober 2026 gegen 14:20 Uhr (spanische Zeit), an dem Tag, an dem ich die erste Version veröffentlichte. Die Idee stammt von MissionControl, mit dem man an der Nintendo Switch Controller anderer Konsolen nutzen kann. AnyPad war vielleicht nicht das erste Projekt, das andere Controller an der PS5 nutzbar machte: Es gab bereits Projekte, die sie per USB verbanden. Aber keines verband sie per Bluetooth, und AnyPad war das erste, das es tat. Ich hatte eine Idee und habe sie umgesetzt.

Das Ziel ist jetzt, dass AnyPad mit mehr Controllern funktioniert. Wenn du es ausprobierst, schick mir bitte dein Log (siehe „Hilf uns, die Kompatibilität zu bestätigen“ weiter unten): Jedes Log hilft.

Wenn du einen Emulator entwickelst und AnyPad integrieren möchtest, bin ich bereit, den Code mit dir zu teilen. Schreib mir an [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com).

Ein bekanntes Problem bleibt: Wird der DualSense ausgeschaltet, während AnyPad läuft, kann er blau blinkend hängen bleiben. Ich hoffe, dass künftige Projekte es nicht haben. Projekte, die auf dem von mir veröffentlichten Code von AnyPad aufbauen (Forks), werden dieses Problem sehr wahrscheinlich erben. Es gehört zum Vermächtnis von AnyPad und ist ein Zeichen dafür, dass sie zumindest in ihrem Bluetooth-Teil Forks von AnyPad sind.

---

## Neu in 0.9

- **Kompatibler:** mehr unterstützte Controller und Unterstützung für die beiden Bluetooth-Chips, die in PS5-Konsolen verbaut sind.
- **Fehlerbehebungen.**
- **Neue grafische Oberfläche** für die AnyPad-Seite.

## Neu in 0.8

### Schnelleres Koppeln

Das Koppeln ist jetzt deutlich schneller und zuverlässiger. In unseren Tests werden der DualShock 4 und der 8BitDo in wenigen Sekunden gefunden und sind wenige Sekunden später bereit, während frühere Versionen oft mehrere Versuche brauchten.

### Stabiler

- **Flüssige Reaktion:** Die Controller reagieren beim Spielen geschmeidig, ohne Verzögerung.
- **Normal eingeschaltete Controller** (statt im Kopplungsmodus) werden jetzt beim ersten Versuch gekoppelt.
- **Klügere Behandlung** von Controllern, die langsam antworten oder einen Schritt ablehnen.

### PS einrichten

Für Controller ohne brauchbare PS-Taste (Switch Pro, 8BitDo und andere) kannst du mit der Schaltfläche **PS einrichten** **eine Taste oder eine Kombination aus drei** wählen, die als PS-Taste dient. Wähle, wie viele, drücke sie am Controller und halte sie eine Sekunde: AnyPad erkennt sie von selbst. Die Einstellung wird für dieses Controller-Modell gespeichert.

DualShock 4 und DualSense haben eine eigene PS-Taste, daher erscheint die Option für sie nicht.

### Zwölf Sprachen

Die AnyPad-Seite folgt der Sprache der Konsole: Englisch, Spanisch, Französisch, Italienisch, Portugiesisch, Deutsch, Russisch, vereinfachtes Chinesisch, traditionelles Chinesisch, Japanisch, Koreanisch und Arabisch (von rechts nach links). Jede andere Sprache fällt auf Englisch zurück.

---

## ⚠️ Wichtig: Schalte den DualSense nicht aus, solange AnyPad läuft

Wird der DualSense ausgeschaltet, während AnyPad läuft, kann er in den blau blinkenden Modus wechseln und nicht mehr funktionieren, bis die Konsole neu gestartet wird.

Damit die Konsole ihn nicht nach einiger Zeit ohne Nutzung selbst ausschaltet, stelle ein:

**Einstellungen → System → Energiesparfunktionen → Zeit bis zum Ausschalten der Controller festlegen → Nicht ausschalten**

Wenn du ihn nicht ausschaltest, funktioniert alles normal.

---

## ⚠️ Wichtig: Wähle „Schnelles Spiel / Gast“ für den externen Controller

Wenn du PS an einem externen Controller drückst, fragt die Konsole, wer ihn benutzt. Wähle **Schnelles Spiel / Gast**. Dein Hauptbenutzer behält den DualSense, und du kannst jederzeit zu ihm zurückkehren, indem du den DualSense wieder in die Hand nimmst.

---

# Controller-Kompatibilität

| Controller | Status | Hinweise |
| --- | --- | --- |
| **DualShock 4** | ✅ **Geprüft** | Versetze ihn bei jedem Koppeln in den Kopplungsmodus (Share + PS). |
| **8BitDo SF30 Pro (Switch-Modus)** | ✅ **Geprüft** | Erscheint als Switch Pro Controller. Nutze PS einrichten für die PS-Taste. |
| **GameSir G7 SE** | ✅ **Geprüft** | |
| **Nintendo Switch Pro Controller** | 🧪 **Unterstützt, noch nicht getestet** | Gleiches Protokoll wie der 8BitDo im Switch-Modus. |
| **Xbox-Controller** | ✅ **Funktionieren (laut Nutzern)** | Mehrere Nutzer bestätigen, dass sie seit 0.8 funktionieren. Wenn deiner sich nicht verbindet, schick mir bitte dein Log und das Modell. |
| **Jedes andere Bluetooth-HID-Gamepad** | 🧪 **Generisches Profil** | Kann ohne spezielles Profil funktionieren. |

**Controller, die wir für kompatibel halten** (im Code unterstützt, noch nicht an einer Konsole getestet): DualShock 4 v1, Hori Onyx, Razer Raiju Ultimate, Razer Raiju Tournament Edition, SCUF Vantage 2, Nintendo Switch Pro Controller, Nintendo Switch Online (SNES, N64, Mega Drive / Genesis), GameSir G3s, G4s, G7 Pro, T1s und T2a, Betop 2585N2, Bionik Vulkan, LanShen X1Pro, Xiaomi Mi Controller, Amazon Fire / iPega, iPega 9-series, 8BitDo SN30 Pro für Xbox Cloud Gaming, 8BitDo Ultimate 2.4G, SteelSeries, NVIDIA-Shield-Controller (2017), Razer Serval, PowerA MOGA Hero, MOGA Pro und MOGA Pro 2, Atari-VCS-Modern-Controller, Hyperkin Scout, Mocute 050. Wenn du einen davon ausprobierst, schick mir bitte dein Log.

Die vollständige Liste, Controller für Controller, steht in [COMPATIBILITY.md](COMPATIBILITY.md).

---

# So funktioniert es

AnyPad PS5 läuft als residenter Payload auf einer PS5 mit Jailbreak. Es nutzt den Bluetooth-Chip der Konsole, **geteilt mit dem System**, um Controller zu finden und zu koppeln, ihre Eingaben zu lesen und sie der PS5 als virtuelle DualSense-Controller darzustellen.

Es verändert **weder** Spiele, **noch** die Firmware der PS5, **noch** den Jailbreak. Deine PS5 muss bereits Payloads ausführen können.

---

# Verwendung

1. **AnyPad laden.** Lade `AnyPad-PS5-0.9.1-beta.elf` von den [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases) herunter und lade es mit dem Payload-Loader, den du üblicherweise nutzt.
2. **Seite öffnen.** Öffne **AnyPad** in der Medien-Zeile der Konsole oder `http://<Konsolen-IP>:8095` von einem Handy oder Computer im selben Netzwerk.
3. **Koppeln.** Drücke **Koppeln** und versetze den Controller dann in den Kopplungsmodus, nahe an der Konsole.

| Controller | Kopplungsmodus |
| --- | --- |
| **DualShock 4** | **Share + PS** halten, bis die Lightbar zweimal blinkt |
| **8BitDo (Switch-Modus)** | **Pair** etwa 3 Sekunden halten, bis die Lichter schnell blinken |
| **Nintendo Switch Pro** | Die kleine Sync-Taste neben dem USB-C-Anschluss drücken |
| **Andere Controller** | Den Anweisungen des Herstellers folgen |

4. **Benutzer wählen.** Wenn die Konsole fragt, wähle **Schnelles Spiel / Gast**.

Mehrere Controller können gleichzeitig zusammen mit dem DualSense verwendet werden.

---

# Jeder Start beginnt sauber

AnyPad speichert gekoppelte Controller nicht zwischen den Starts. Koppel deine Controller jedes Mal, wenn du AnyPad startest. So vermeidest du alte Kopplungen, die nicht mehr funktionieren, und der Bluetooth-Chip bleibt in einem bekannten Zustand.

Verbindet sich ein ausgeschalteter Controller von selbst wieder und reagiert nicht, schalte ihn aus und koppel ihn über die Seite neu.

---

# AnyPad beenden

Beende es immer mit **AnyPad PS5 beenden** auf seiner Seite, auch bevor du es erneut startest und bevor du die Konsole ausschaltest oder neu startest. Die Schaltfläche **Verlassen** schließt nur die Seite.

---

# Fehlerbehebung

### Es wird kein Controller gefunden

1. Drücke zuerst **Koppeln** und versetze den Controller danach in den Kopplungsmodus.
2. Prüfe, ob der Controller wirklich im Kopplungsmodus ist (z. B. muss die Lightbar des DualShock 4 zweimal blinken).
3. Wurde der Controller zuvor mit einem anderen Gerät gekoppelt und erscheint trotzdem nicht, setze ihn zurück (DualShock 4: Büroklammer in das kleine Loch neben L2, etwa 5 Sekunden; 8BitDo Pro 2: Pair etwa 5 Sekunden halten) und versuche es erneut.

### Ein anderer Controller-Payload läuft

Ein anderer Payload, der den Bluetooth-Chip und Port 8095 nutzt, stört. Beende ihn über sein eigenes Menü, bevor du AnyPad startest; AnyPad zeigt einen Hinweis, wenn es das erkennt.

### Die Seite öffnet sich nicht

Prüfe, ob AnyPad läuft, ob dein Handy oder Computer im selben Netzwerk wie die PS5 ist und ob du die IP-Adresse der Konsole mit Port `8095` verwendest.

---

# Hilf uns, die Kompatibilität zu bestätigen

Wenn du bestätigen kannst, dass AnyPad PS5 mit einem Controller funktioniert (oder nicht), sende das Log (`/data/anypad/anypad.log`) an [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com), mit Controller-Modell, Konsolen-Firmware, Jailbreak und AnyPad-Version. Das hilft sehr.

### Das Projekt unterstützen

Viele Leute bitten mich, andere Controller und andere Konsolenversionen zu testen. Jede Hilfe ist willkommen, um **dieses Projekt auf die PS4** und auf die **PS5-Firmware 13.60** zu portieren und **mehr Controller** zu unterstützen. Ich hätte gern eine Konsole mit Firmware 13.60 zusätzlich zu meiner mit 10.01. Es ist nicht verpflichtend, aber wer dem Projekt helfen möchte, kann das auf meiner Throne-Seite tun: **[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# Danksagung

AnyPad PS5 ist Originalcode. Diese Projekte wurden studiert, um zu verstehen, wie die Dinge funktionieren, und ihre Ideen wurden für AnyPad neu geschrieben. Dank an ihre Autoren:

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore): die Idee des Bluetooth-Teils, den Bluetooth-Chip der PS5 mit dem System zu teilen.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): Controller-Profile nach Hersteller und Modell sowie das Berichtsformat vieler Controller.
- **Ghostcontrol** (StonedModder): wie man auf der PS5 einen virtuellen DualSense erzeugt und einem Benutzer zuweist.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) und **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: die Abfolge des virtuellen Controllers und das Datenformat des Controllers.
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)** (X-F1REBALL-X): Ideen zur Bluetooth-Aushandlung.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom): die Werkzeuge, mit denen alles gebaut wird.

Lizenz: GPL-3.0-or-later ([LICENSE](LICENSE)).

Autor: **elmonomalvad0**, derselbe Autor wie *sinfiltros* (das ursprüngliche AnyPad PS5 auf GitHub).
X: [@elmonomalvad0](https://x.com/elmonomalvad0) · Kontakt: exposed.ct.ws@gmail.com
