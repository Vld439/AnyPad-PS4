<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS5"></p>

# AnyPad PS5: libera la tua PS5 dal DualSense

**Un ponte per controller Bluetooth sulle PS5 con jailbreak.**

[English · Español](README.md) · [Français](README.fr.md) · **Italiano** · [Português](README.pt.md) · [Deutsch](README.de.md) · [Русский](README.ru.md) · [简体中文](README.zh.md) · [繁體中文](README.zh-Hant.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md)

---

AnyPad PS5 ti permette di usare sulla tua PS5 controller di altre piattaforme. Li collega tramite il chip Bluetooth della console e li presenta al sistema come DualSense virtuali.

Non serve nessun adattatore USB.

**Versione 0.9.5-beta.** Provata su una PS5 fat, firmware 10.01, con un DualShock 4 e un 8BitDo SF30 Pro (modalità Switch), nei menu della console, nei giochi e negli emulatori.

**Disponibile in dodici lingue:** inglese, spagnolo, francese, italiano, portoghese, tedesco, russo, cinese semplificato, cinese tradizionale, giapponese, coreano e arabo. La pagina di AnyPad segue la lingua della console.

---

## Su questo progetto

AnyPad PS5 è nato il 3 ottobre 2026 verso le 14:20 (ora spagnola), il giorno in cui ho pubblicato la prima versione. L'idea viene da MissionControl, che permette di usare sulla Nintendo Switch i controller di altre console. Forse AnyPad non è stato il primo progetto per usare altri controller sulla PS5: esistevano già progetti che li collegavano via USB. Ma nessuno li collegava via Bluetooth, e AnyPad è stato il primo a farlo. Ho avuto un'idea e l'ho realizzata.

L'obiettivo ora è far funzionare AnyPad con più controller. Se lo provi, inviami il tuo log (vedi «Aiutaci a confermare la compatibilità» più sotto): ogni log aiuta.

Il codice sorgente completo è pubblicato in questo repository con licenza GPL-3.0-or-later (vedi «Codice aperto e lavoro con un'IA» più sotto). Se sviluppi un emulatore e vuoi integrare AnyPad, puoi farlo con quella licenza. Scrivimi a [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com) se ti serve aiuto.

Rimane un problema noto: se il DualSense viene spento mentre AnyPad è in funzione, può restare con la luce blu lampeggiante. Spero che i progetti futuri non lo abbiano. I progetti derivati da AnyPad, dal codice che ho pubblicato, molto probabilmente erediteranno questo problema. Fa parte dell'eredità di AnyPad, ed è un segno che, almeno nella parte Bluetooth, sono fork di AnyPad.

---

## Codice aperto e lavoro con un'IA

Dalla versione 0.9.5 tutto il codice sorgente di AnyPad PS5 è pubblicato in questo repository, con licenza GPL-3.0-or-later: l'host Bluetooth, i profili dei controller, la pagina di AnyPad, i test e gli script di compilazione. Include un simulatore, quindi si può verificare un controller su un computer senza console.

Quello che secondo me AnyPad aggiunge davvero, più del ponte in sé, è **il suo sistema di comunicazione per integrare nuovi controller con l'aiuto di un'IA**. Mentre AnyPad è in esecuzione, la sua pagina offre sulla tua rete locale:

- il log, in tempo reale;
- alcune impostazioni della connessione Bluetooth che si possono cambiare a caldo, senza riavviare né ricompilare;
- il pulsante di associazione.

Un assistente IA che abbia un terminale su un computer della stessa rete (per esempio Claude Code, Codex CLI o Gemini CLI) può seguire ciò che fa AnyPad mentre accendi un controller, proporre una modifica, ricompilare l'ELF, caricarlo sulla console e controllare il risultato alla prova successiva. È così che è stato migliorato AnyPad stesso.

Il file **[AI-WORKFLOW.md](AI-WORKFLOW.md)** (in inglese) è una guida scritta per quell'assistente: come collegarsi, cosa può e cosa non può toccare, come provare una modifica e come aggiungere un nuovo controller. Puoi darlo all'assistente come prima istruzione.

Tieni presente che:

- Una chat nel browser non raggiunge la tua console. L'assistente ha bisogno di un terminale su un computer collegato alla stessa rete locale della PS5. AnyPad non si connette mai a internet.
- La pagina non ha PIN, come in tutte le versioni: chiunque sulla tua rete locale può leggere il log e inviare comandi ad AnyPad. Usalo solo su una rete fidata e ferma AnyPad quando hai finito.
- Il log contiene gli indirizzi Bluetooth dei tuoi controller e l'id del tuo utente della console. Controllalo prima di condividerlo.

### Condividi quello che imparate (tu e la tua IA)

Se adatti AnyPad a un controller, con o senza IA, apri per favore una [segnalazione](https://codeberg.org/elmonomalva0/Any-Pad-ps5/issues) con il modello del controller, il firmware della console, il jailbreak, la versione di AnyPad, che cosa hai cambiato e perché, e il log. Se ti ha aiutato un'IA, dillo e lascia che scriva lei la spiegazione: che cosa fa di diverso il controller, che cosa è stato cambiato e come è stato provato. Esaminerò ogni proposta e integrerò quelle compatibili nelle versioni successive. Una modifica deve continuare a superare `make test` e non deve alterare il modo in cui si collegano il DualShock 4 e l'8BitDo.

### Compilare dal codice

Ti serve il [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk).

```
make test                                               # test sul computer, con il simulatore Bluetooth
make ps5 PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk       # l'ELF in dist/
tools/build-elf-docker.sh /path/to/ps5-payload-sdk      # oppure lo stesso, dentro un container
```

### Per favore, menzionami

Se crei una tua versione di AnyPad, oppure la includi (o ne includi una parte) in un emulatore, un HEN, un payload o qualsiasi altro programma, te ne sarei molto grato se menzionassi **elmonomalvad0** e inserissi un link a questo progetto. La licenza, GPL-3.0-or-later, richiede già di mantenere gli avvisi di copyright e di distribuire le tue modifiche con la stessa licenza. La menzione è una richiesta, non una condizione, ed è ciò che fa valere la pena continuare a condividere il lavoro.

### Dentro il codice

| Dove | Che cosa fa |
| --- | --- |
| `src/host.c` | L'host Bluetooth Classic: ricerca, chiamate, associazione, canali L2CAP, SDP e HID |
| `src/le.c` | Bluetooth LE: ricerca, associazione con Secure Connections e scambio di identità, HID su GATT |
| `src/profiles.c` | Un profilo per famiglia di controller: id, come leggere i report, sequenza di risveglio, vibrazione e luci |
| `src/generic.c`, `maps/` | Qualsiasi altro controller HID, letto tramite il proprio descrittore e un file di mappatura facoltativo |
| `src/hci_usb.c`, `src/usb_desc.c`, `src/evstream.c` | L'HCI via USB con il chip Bluetooth della console, condiviso con il sistema |
| `src/ps5_vpad.c` | Crea il DualSense virtuale e lo alimenta |
| `src/web.c`, `web/index.html` | La pagina e la sua API |
| `src/ps5_main.c` | Il ciclo principale del payload |
| `tests/` | I simulatori Bluetooth (`sim.c`, `sim_le.c`) e tutti i test |

Aggiungere un controller significa, nella maggior parte dei casi, aggiungere un profilo in `src/profiles.c` e un test con il simulatore. [AI-WORKFLOW.md](AI-WORKFLOW.md) spiega i dettagli.

---

## Novità della 0.9.5

- **Codice aperto:** tutto il codice è pubblicato, con una guida per lavorare con un assistente IA.

## Novità della 0.9

- **Più compatibile:** supporta più controller ed è compatibile con i due chip Bluetooth presenti nelle PS5.
- **Correzione di bug.**
- **Nuova interfaccia grafica** per la pagina di AnyPad.

## Novità della 0.8

### Associazione più rapida

L'associazione ora è molto più rapida e affidabile. Nelle nostre prove il DualShock 4 e l'8BitDo vengono trovati in pochi secondi e sono pronti pochi secondi dopo, mentre prima spesso servivano più tentativi.

### Più stabile

- **Risposta fluida:** i controller reagiscono con scioltezza mentre giochi, senza lag.
- **Controller accesi normalmente** (invece che in modalità di associazione): ora si associano al primo tentativo.
- **Gestione più intelligente** dei controller che tardano a rispondere o rifiutano un passaggio.

### Configura PS

Per i controller senza un tasto PS utilizzabile (Switch Pro, 8BitDo e altri), il pulsante **Configura PS** permette di scegliere **un tasto o una combinazione di tre** che farà da tasto PS. Scegli quanti, premili sul controller e tienili premuti per un secondo: AnyPad li rileva da solo. L'impostazione viene salvata per quel modello di controller.

I DualShock 4 e i DualSense hanno il loro tasto PS, quindi per loro questa opzione non compare.

### Dodici lingue

La pagina di AnyPad segue la lingua della console: inglese, spagnolo, francese, italiano, portoghese, tedesco, russo, cinese semplificato, cinese tradizionale, giapponese, coreano e arabo (da destra a sinistra). Qualsiasi altra lingua usa l'inglese.

---

## ⚠️ Importante: non spegnere il DualSense mentre AnyPad è in funzione

Se il DualSense viene spento mentre AnyPad è in funzione, può entrare nella modalità blu lampeggiante e non funzionare fino al riavvio della console.

Perché la console non lo spenga da sola dopo un po' di inattività, imposta:

**Impostazioni → Sistema → Risparmio energetico → Imposta il tempo per lo spegnimento dei controller → Non spegnere**

Se non lo spegni, tutto funziona normalmente.

---

## ⚠️ Importante: scegli «Gioco rapido / Ospite» per il controller esterno

Quando premi PS su un controller esterno, la console chiede chi lo userà. Scegli **Gioco rapido / Ospite**. Il tuo utente principale tiene il DualSense, e puoi tornarci in qualsiasi momento riprendendo il DualSense.

---

# Compatibilità dei controller

| Controller | Stato | Note |
| --- | --- | --- |
| **DualShock 4** | ✅ **Verificato** | Mettilo in modalità di associazione (Share + PS) ogni volta che lo associ. |
| **8BitDo SF30 Pro (modalità Switch)** | ✅ **Verificato** | Appare come Switch Pro Controller. Usa Configura PS per il tasto PS. |
| **GameSir G7 SE** | ✅ **Verificato** | |
| **Nintendo Switch Pro Controller** | 🧪 **Supportato, non ancora provato** | Stesso protocollo dell'8BitDo in modalità Switch. |
| **Controller Xbox** | ✅ **Funzionano (secondo gli utenti)** | Alcuni utenti confermano che funzionano dalla 0.8. Se il tuo non si connette, inviami il tuo log e il modello. |
| **Qualsiasi altro controller HID Bluetooth** | 🧪 **Profilo generico** | Può funzionare senza un profilo specifico. |

**Controller che pensiamo siano compatibili** (supportati nel codice, non ancora provati su una console): DualShock 4 v1, Hori Onyx, Razer Raiju Ultimate, Razer Raiju Tournament Edition, SCUF Vantage 2, Nintendo Switch Pro Controller, Nintendo Switch Online (SNES, N64, Mega Drive / Genesis), GameSir G3s, G4s, G7 Pro, T1s e T2a, Betop 2585N2, Bionik Vulkan, LanShen X1Pro, Xiaomi Mi Controller, Amazon Fire / iPega, iPega 9-series, 8BitDo SN30 Pro per Xbox Cloud Gaming, 8BitDo Ultimate 2.4G, SteelSeries, controller NVIDIA Shield (2017), Razer Serval, PowerA MOGA Hero, MOGA Pro e MOGA Pro 2, controller Atari VCS Modern, Hyperkin Scout, Mocute 050. Se ne provi uno, inviami il tuo log.

L'elenco completo, controller per controller, è in [COMPATIBILITY.md](COMPATIBILITY.md).

---

# Come funziona

AnyPad PS5 viene eseguito come payload residente su una PS5 con jailbreak. Usa il chip Bluetooth della console, **condividendolo con il sistema**, per trovare e associare i controller, leggerne i comandi e presentarli alla PS5 come DualSense virtuali.

**Non** modifica i giochi, il firmware della PS5 né il jailbreak. La tua PS5 deve già poter eseguire payload.

---

# Come si usa

1. **Carica AnyPad.** Scarica `AnyPad-PS5-0.9.5-beta.elf` dalle [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases) e caricalo con il caricatore di payload che usi di solito.
2. **Apri la pagina.** Apri **AnyPad** nella riga Contenuti multimediali della console, oppure `http://<ip-della-console>:8095` da un telefono o un computer della stessa rete.
3. **Associa.** Premi **Associa**, poi metti il controller in modalità di associazione, vicino alla console.

| Controller | Modalità di associazione |
| --- | --- |
| **DualShock 4** | Tieni premuti **Share + PS** finché la barra luminosa lampeggia due volte |
| **8BitDo (modalità Switch)** | Tieni premuto **Pair** per circa 3 secondi, finché le luci lampeggiano velocemente |
| **Nintendo Switch Pro** | Premi il piccolo pulsante di sincronizzazione accanto alla porta USB-C |
| **Altri controller** | Segui le istruzioni del produttore |

4. **Scegli l'utente.** Quando la console lo chiede, scegli **Gioco rapido / Ospite**.

Si possono usare più controller insieme, oltre al DualSense.

---

# Ogni avvio parte da zero

AnyPad non conserva i controller associati tra un avvio e l'altro. Associa i tuoi controller ogni volta che avvii AnyPad. Così si evitano vecchie associazioni che non funzionano più e il chip Bluetooth resta in uno stato noto.

Se un controller che era stato spento si ricollega da solo e non risponde, spegnilo e associalo di nuovo dalla pagina.

---

# Arrestare AnyPad

Arrestalo sempre con **Arresta AnyPad PS5** nella sua pagina, anche prima di avviarlo di nuovo e prima di spegnere o riavviare la console. Il pulsante **Esci** chiude solo la pagina.

---

# Risoluzione dei problemi

### Non trova nessun controller

1. Premi prima **Associa** e poi metti il controller in modalità di associazione.
2. Controlla che il controller sia davvero in modalità di associazione (per esempio, la barra luminosa del DualShock 4 deve lampeggiare due volte).
3. Se il controller è stato associato prima a un altro dispositivo e continua a non comparire, reimpostalo (DualShock 4: una graffetta nel forellino accanto a L2 per circa 5 secondi; 8BitDo Pro 2: tieni premuto Pair per circa 5 secondi) e riprova.

### È in funzione un altro payload per controller

Un altro payload che usa il chip Bluetooth e la porta 8095 interferisce. Arrestalo dal suo menu prima di avviare AnyPad; AnyPad mostra un avviso quando lo rileva.

### La pagina non si apre

Controlla che AnyPad sia in funzione, che il telefono o il computer sia sulla stessa rete della PS5 e di usare l'indirizzo IP della console con la porta `8095`.

---

# Aiutaci a confermare la compatibilità

Se puoi confermare che AnyPad PS5 funziona (o non funziona) con un controller, invia il log (`/data/anypad/anypad.log`) a [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com), con il modello del controller, il firmware della console, il jailbreak e la versione di AnyPad. Aiuta molto.

### Sostieni il progetto

Molte persone mi chiedono di provare altri controller e altre versioni della console. Ogni aiuto è benvenuto per **portare questo progetto su PS4** e sul **firmware 13.60 della PS5**, e per supportare **più controller**. Mi piacerebbe molto avere una console con il firmware 13.60 oltre alla mia con il 10.01. Non è obbligatorio, ma chi vuole aiutare il progetto può farlo dalla mia pagina Throne: **[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# Ringraziamenti

AnyPad PS5 è codice originale. Questi progetti sono stati studiati per capire come funzionano le cose, e le loro idee sono state riscritte per AnyPad. Grazie ai loro autori:

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore): l'idea della parte Bluetooth, condividere il chip Bluetooth della PS5 con il sistema.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): i profili dei controller per produttore e modello, e il formato dei report di molti controller.
- **Ghostcontrol** (StonedModder): come creare un DualSense virtuale sulla PS5 e assegnarlo a un utente.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) e **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: la sequenza del controller virtuale e il formato dei dati del controller.
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)** (X-F1REBALL-X): idee sulla negoziazione Bluetooth.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom): gli strumenti con cui si compila tutto.

Licenza: GPL-3.0-or-later ([LICENSE](LICENSE)).

Autore: **elmonomalvad0**, lo stesso autore di *sinfiltros* (l'AnyPad PS5 originale su GitHub).
X: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contatto: exposed.ct.ws@gmail.com
