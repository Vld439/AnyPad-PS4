<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS5"></p>

# AnyPad PS5 : libérez votre PS5 de la DualSense

**Un pont pour manettes Bluetooth sur les PS5 jailbreakées.**

[English · Español](README.md) · **Français** · [Italiano](README.it.md) · [Português](README.pt.md) · [Deutsch](README.de.md) · [Русский](README.ru.md) · [简体中文](README.zh.md) · [繁體中文](README.zh-Hant.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md)

---

AnyPad PS5 vous permet d'utiliser sur votre PS5 des manettes d'autres plateformes. Il les connecte par la puce Bluetooth de la console et les présente au système comme des DualSense virtuelles.

Aucun adaptateur USB n'est nécessaire.

**Version 0.9.5-beta.** Testée sur une PS5 fat, firmware 10.01, avec une DualShock 4 et une 8BitDo SF30 Pro (mode Switch), dans les menus de la console, dans les jeux et dans les émulateurs.

**Disponible en douze langues :** anglais, espagnol, français, italien, portugais, allemand, russe, chinois simplifié, chinois traditionnel, japonais, coréen et arabe. La page d'AnyPad suit la langue de la console.

---

## À propos de ce projet

AnyPad PS5 a commencé le 3 octobre 2026 vers 14 h 20 (heure espagnole), le jour où j'ai publié sa première version. L'idée vient de MissionControl, qui permet d'utiliser sur la Nintendo Switch des manettes d'autres consoles. AnyPad n'a peut-être pas été le premier projet à utiliser d'autres manettes sur la PS5 : il existait déjà des projets qui les connectaient en USB. Mais aucun ne les connectait en Bluetooth, et AnyPad a été le premier à le faire. J'ai eu une idée et je l'ai menée à bien.

L'objectif est maintenant que AnyPad fonctionne avec davantage de manettes. Si vous l'essayez, envoyez-moi votre journal (voir « Aidez-nous à confirmer la compatibilité » plus bas) : chaque journal aide.

Le code source complet est publié dans ce dépôt sous licence GPL-3.0-or-later (voir « Code ouvert et travail avec une IA » plus bas). Si vous développez un émulateur et souhaitez intégrer AnyPad, vous pouvez le faire sous cette licence. Écrivez-moi à [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com) si vous avez besoin d'aide.

Il reste un problème connu : si la DualSense est éteinte pendant que AnyPad fonctionne, elle peut rester en bleu clignotant. J'espère que les futurs projets ne l'auront pas. Les projets dérivés d'AnyPad, à partir du code que j'ai publié, hériteront très probablement de ce problème. Cela fait partie de l'héritage d'AnyPad, et c'est un signe qu'au moins pour leur partie Bluetooth, ce sont des forks d'AnyPad.

---

## Code ouvert et travail avec une IA

Depuis la version 0.9.5, tout le code source d'AnyPad PS5 est publié dans ce dépôt, sous licence GPL-3.0-or-later : l'hôte Bluetooth, les profils de manettes, la page d'AnyPad, les tests et les scripts de compilation. Il comprend un simulateur : on peut vérifier une manette sur un ordinateur, sans console.

Ce que, selon moi, AnyPad apporte vraiment, plus que le pont lui-même, c'est **son système de communication pour intégrer de nouvelles manettes avec l'aide d'une IA**. Tant qu'AnyPad fonctionne, sa page propose sur votre réseau local :

- le journal, en temps réel ;
- quelques réglages de la connexion Bluetooth modifiables à chaud, sans redémarrer ni recompiler ;
- le bouton d'appairage.

Un assistant IA disposant d'un terminal sur un ordinateur du même réseau (par exemple Claude Code, Codex CLI ou Gemini CLI) peut suivre ce que fait AnyPad pendant que vous allumez une manette, proposer une modification, recompiler l'ELF, l'envoyer à la console et vérifier le résultat à l'exécution suivante. C'est ainsi qu'AnyPad lui-même a été amélioré.

Le fichier **[AI-WORKFLOW.md](AI-WORKFLOW.md)** (en anglais) est un guide écrit pour cet assistant : comment se connecter, ce qu'il peut et ne peut pas toucher, comment tester une modification et comment ajouter une nouvelle manette. Vous pouvez le donner à l'assistant comme première instruction.

À garder à l'esprit :

- Un chat dans un navigateur ne peut pas atteindre votre console. L'assistant a besoin d'un terminal sur un ordinateur connecté au même réseau local que la PS5. AnyPad ne se connecte jamais à internet.
- La page n'a pas de code PIN, comme dans toutes les versions : toute personne de votre réseau local peut lire le journal et envoyer des commandes à AnyPad. Utilisez-le uniquement sur un réseau de confiance et arrêtez AnyPad quand vous avez terminé.
- Le journal contient les adresses Bluetooth de vos manettes et l'identifiant de votre utilisateur de la console. Relisez-le avant de le partager.

### Partagez ce que vous (et votre IA) apprenez

Si vous adaptez AnyPad à une manette, avec ou sans IA, ouvrez s'il vous plaît un [ticket](https://codeberg.org/elmonomalva0/Any-Pad-ps5/issues) avec le modèle de la manette, le firmware de la console, le jailbreak, la version d'AnyPad, ce que vous avez modifié et pourquoi, ainsi que le journal. Si une IA vous a aidé, dites-le et laissez-la rédiger l'explication : ce que la manette fait différemment, ce qui a été modifié et comment cela a été testé. J'examinerai chaque proposition et j'intégrerai les compatibles dans les versions suivantes. Une modification doit continuer à réussir `make test` et ne doit pas changer la façon dont la DualShock 4 et la 8BitDo se connectent.

### Compiler depuis le code

Il vous faut le [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk).

```
make test                                               # tests sur l'ordinateur, avec le simulateur Bluetooth
make ps5 PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk       # l'ELF dans dist/
tools/build-elf-docker.sh /path/to/ps5-payload-sdk      # ou la même chose, dans un conteneur
```

### Merci de me mentionner

Si vous réalisez votre propre version d'AnyPad, ou si vous l'intégrez (en tout ou en partie) dans un émulateur, un HEN, un payload ou tout autre programme, je vous serais très reconnaissant de mentionner **elmonomalvad0** et de faire un lien vers ce projet. La licence, GPL-3.0-or-later, exige déjà de conserver les mentions de copyright et de distribuer vos modifications sous la même licence. La mention est une demande, pas une condition, et c'est ce qui donne envie de continuer à partager le travail.

### Dans le code

| Où | Ce que cela fait |
| --- | --- |
| `src/host.c` | L'hôte Bluetooth Classic : recherche, appels, appairage, canaux L2CAP, SDP et HID |
| `src/le.c` | Bluetooth LE : recherche, appairage Secure Connections et échange d'identité, HID sur GATT |
| `src/profiles.c` | Un profil par famille de manettes : identifiants, lecture des rapports, séquence de réveil, vibration et lumières |
| `src/generic.c`, `maps/` | Toute autre manette HID, lue grâce à son propre descripteur et à un fichier de mappage facultatif |
| `src/hci_usb.c`, `src/usb_desc.c`, `src/evstream.c` | Le HCI par USB avec la puce Bluetooth de la console, partagée avec le système |
| `src/ps5_vpad.c` | Crée la DualSense virtuelle et l'alimente |
| `src/web.c`, `web/index.html` | La page et son API |
| `src/ps5_main.c` | La boucle principale du payload |
| `tests/` | Les simulateurs Bluetooth (`sim.c`, `sim_le.c`) et tous les tests |

Ajouter une manette revient, la plupart du temps, à ajouter un profil dans `src/profiles.c` et un test avec le simulateur. [AI-WORKFLOW.md](AI-WORKFLOW.md) donne les détails.

---

## Nouveautés de la 0.9.5

- **Code ouvert :** tout le code est publié, avec un guide pour travailler avec un assistant IA.

## Nouveautés de la 0.9

- **Plus compatible :** davantage de manettes prises en charge, et compatibilité avec les deux puces Bluetooth présentes dans les PS5.
- **Corrections de bugs.**
- **Nouvelle interface graphique** pour la page d'AnyPad.

## Nouveautés de la 0.8

### Association plus rapide

L'association est maintenant beaucoup plus rapide et fiable. Dans nos essais, la DualShock 4 et la 8BitDo sont trouvées en quelques secondes et prêtes quelques secondes plus tard, alors qu'avant il fallait souvent plusieurs tentatives.

### Plus stable

- **Réponse fluide :** les manettes réagissent avec souplesse pendant le jeu, sans latence.
- **Manettes allumées normalement** (au lieu du mode association) : elles sont maintenant associées dès la première tentative.
- **Gestion plus intelligente** des manettes qui tardent à répondre ou refusent une étape.

### Configurer PS

Pour les manettes sans touche PS utilisable (Switch Pro, 8BitDo et autres), le bouton **Configurer PS** permet de choisir **une touche ou une combinaison de trois** qui servira de touche PS. Choisissez combien, appuyez dessus sur la manette et maintenez-les une seconde : AnyPad les détecte tout seul. Le réglage est enregistré pour ce modèle de manette.

Les DualShock 4 et DualSense ont leur propre touche PS, cette option n'apparaît donc pas pour elles.

### Douze langues

La page d'AnyPad suit la langue de la console : anglais, espagnol, français, italien, portugais, allemand, russe, chinois simplifié, chinois traditionnel, japonais, coréen et arabe (de droite à gauche). Toute autre langue utilise l'anglais.

---

## ⚠️ Important : n'éteignez pas la DualSense pendant que AnyPad fonctionne

Si la DualSense est éteinte pendant que AnyPad fonctionne, elle peut passer en bleu clignotant et ne plus fonctionner jusqu'au redémarrage de la console.

Pour que la console ne l'éteigne pas toute seule après un moment sans utilisation, réglez :

**Paramètres → Système → Économie d'énergie → Délai avant l'arrêt des manettes → Ne pas éteindre**

Si vous ne l'éteignez pas, tout fonctionne normalement.

---

## ⚠️ Important : choisissez « Partie rapide / Invité » pour la manette externe

Quand vous appuyez sur PS sur une manette externe, la console demande qui va l'utiliser. Choisissez **Partie rapide / Invité**. Votre utilisateur principal garde la DualSense, et vous pouvez y revenir à tout moment en reprenant la DualSense.

---

# Compatibilité des manettes

| Manette | État | Remarques |
| --- | --- | --- |
| **DualShock 4** | ✅ **Vérifiée** | Mettez-la en mode association (Share + PS) à chaque association. |
| **8BitDo SF30 Pro (mode Switch)** | ✅ **Vérifiée** | Elle apparaît comme une Switch Pro Controller. Utilisez Configurer PS pour la touche PS. |
| **GameSir G7 SE** | ✅ **Vérifiée** | |
| **Manette Nintendo Switch Pro** | 🧪 **Prise en charge, pas encore testée** | Même protocole que la 8BitDo en mode Switch. |
| **Manettes Xbox** | ✅ **Fonctionnent (selon les utilisateurs)** | Des utilisateurs confirment qu'elles fonctionnent depuis la 0.8. Si la vôtre ne se connecte pas, envoyez-moi votre journal et son modèle. |
| **Toute autre manette HID Bluetooth** | 🧪 **Profil générique** | Peut fonctionner sans profil spécifique. |

**Manettes que nous pensons compatibles** (prises en charge dans le code, pas encore testées sur une console) : DualShock 4 v1, Hori Onyx, Razer Raiju Ultimate, Razer Raiju Tournament Edition, SCUF Vantage 2, Nintendo Switch Pro Controller, Nintendo Switch Online (SNES, N64, Mega Drive / Genesis), GameSir G3s, G4s, G7 Pro, T1s et T2a, Betop 2585N2, Bionik Vulkan, LanShen X1Pro, Xiaomi Mi Controller, Amazon Fire / iPega, iPega 9-series, 8BitDo SN30 Pro pour Xbox Cloud Gaming, 8BitDo Ultimate 2.4G, SteelSeries, manette NVIDIA Shield (2017), Razer Serval, PowerA MOGA Hero, MOGA Pro et MOGA Pro 2, manette Atari VCS Modern, Hyperkin Scout, Mocute 050. Si vous en essayez une, envoyez-moi votre journal.

La liste complète, manette par manette, se trouve dans [COMPATIBILITY.md](COMPATIBILITY.md).

---

# Comment ça marche

AnyPad PS5 s'exécute comme un payload résident sur une PS5 jailbreakée. Il utilise la puce Bluetooth de la console, **en la partageant avec le système**, pour trouver et associer des manettes, lire leurs commandes et les présenter à la PS5 comme des DualSense virtuelles.

Il **ne** modifie **ni** les jeux, **ni** le firmware de la PS5, **ni** le jailbreak. Votre PS5 doit déjà pouvoir exécuter des payloads.

---

# Utilisation

1. **Chargez AnyPad.** Téléchargez `AnyPad-PS5-0.9.5-beta.elf` depuis les [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases) et chargez-le avec votre chargeur de payloads habituel.
2. **Ouvrez la page.** Ouvrez **AnyPad** dans la rangée Médias de la console, ou `http://<ip-de-la-console>:8095` depuis un téléphone ou un ordinateur du même réseau.
3. **Associez.** Appuyez sur **Associer**, puis mettez la manette en mode association, près de la console.

| Manette | Mode association |
| --- | --- |
| **DualShock 4** | Maintenez **Share + PS** jusqu'à ce que la barre lumineuse clignote deux fois |
| **8BitDo (mode Switch)** | Maintenez **Pair** environ 3 secondes, jusqu'à ce que les voyants clignotent vite |
| **Nintendo Switch Pro** | Appuyez sur le petit bouton de synchronisation près du port USB-C |
| **Autres manettes** | Suivez les instructions du fabricant |

4. **Choisissez l'utilisateur.** Quand la console le demande, choisissez **Partie rapide / Invité**.

Plusieurs manettes peuvent être utilisées en même temps, avec la DualSense.

---

# Chaque exécution part de zéro

AnyPad ne garde pas les manettes associées d'une exécution à l'autre. Associez vos manettes à chaque démarrage d'AnyPad. Cela évite les anciennes associations qui ne fonctionnent plus et laisse la puce Bluetooth dans un état connu.

Si une manette qui avait été éteinte se reconnecte toute seule et ne répond pas, éteignez-la et associez-la de nouveau depuis la page.

---

# Arrêter AnyPad

Arrêtez-le toujours avec **Arrêter AnyPad PS5** sur sa page, y compris avant de le relancer et avant d'éteindre ou de redémarrer la console. Le bouton **Quitter** ferme seulement la page.

---

# Dépannage

### Aucune manette n'est trouvée

1. Appuyez d'abord sur **Associer**, puis mettez la manette en mode association.
2. Vérifiez que la manette est vraiment en mode association (par exemple, la barre lumineuse de la DualShock 4 doit clignoter deux fois).
3. Si la manette a déjà été associée à un autre appareil et n'apparaît toujours pas, réinitialisez-la (DualShock 4 : un trombone dans le petit trou près de L2 pendant environ 5 secondes ; 8BitDo Pro 2 : maintenez Pair environ 5 secondes) et réessayez.

### Un autre payload de manettes est en cours d'exécution

Un autre payload qui utilise la puce Bluetooth et le port 8095 gêne AnyPad. Arrêtez-le depuis son propre menu avant de lancer AnyPad ; AnyPad affiche un avertissement quand il le détecte.

### La page ne s'ouvre pas

Vérifiez qu'AnyPad fonctionne, que votre téléphone ou ordinateur est sur le même réseau que la PS5 et que vous utilisez l'adresse IP de la console avec le port `8095`.

---

# Aidez-nous à confirmer la compatibilité

Si vous pouvez confirmer que AnyPad PS5 fonctionne (ou ne fonctionne pas) avec une manette, envoyez le journal (`/data/anypad/anypad.log`) à [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com), avec le modèle de la manette, le firmware de la console, le jailbreak et la version d'AnyPad. Cela aide beaucoup.

### Soutenir le projet

Beaucoup de personnes me demandent d'essayer d'autres manettes et d'autres versions de la console. Toute aide est la bienvenue pour **porter ce projet sur PS4** et sur le **firmware 13.60 de la PS5**, et pour prendre en charge **davantage de manettes**. J'aimerais beaucoup avoir une console en firmware 13.60 en plus de la mienne en 10.01. Ce n'est pas obligatoire, mais ceux qui veulent aider le projet peuvent le faire sur ma page Throne : **[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# Remerciements

AnyPad PS5 est un code original. Ces projets ont été étudiés pour comprendre le fonctionnement des choses, et leurs idées ont été réécrites pour AnyPad. Merci à leurs auteurs :

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore) : l'idée de la partie Bluetooth, partager la puce Bluetooth de la PS5 avec le système.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly) : les profils de manettes par fabricant et modèle, et le format des rapports de nombreuses manettes.
- **Ghostcontrol** (StonedModder) : comment créer une DualSense virtuelle sur la PS5 et l'attribuer à un utilisateur.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) et **[shadPS4](https://github.com/shadps4-emu/shadPS4)** : la séquence de la manette virtuelle et le format des données de la manette.
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)** (X-F1REBALL-X): des idées sur la négociation Bluetooth.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom) : les outils avec lesquels tout est compilé.

Licence : GPL-3.0-or-later ([LICENSE](LICENSE)).

Auteur : **elmonomalvad0**, le même auteur que *sinfiltros* (l'AnyPad PS5 original sur GitHub).
X : [@elmonomalvad0](https://x.com/elmonomalvad0) · Contact : exposed.ct.ws@gmail.com
