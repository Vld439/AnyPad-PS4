<p align="center"><img src="assets/icon0.png" width="200" alt="AnyPad PS5"></p>

# AnyPad PS5

**Connect your Bluetooth controllers to your PS5 console.**
**Conecta tus mandos Bluetooth a tu consola PS5.**

**[English](#english) · [Español](#español)**

---

<a name="english"></a>
## 🇬🇧 English

Use other Bluetooth controllers on a jailbroken PS5 (DualShock 4, 8BitDo, Switch Pro and more), with no
dongle. They show up in the console and in games as a DualSense.

**Version 0.5.28-beta (6 October 2026).** Tested on a PS5 fat, firmware 10.01, with a DualShock 4 and an
8BitDo SF30 Pro (Switch mode).

### How to use it

1. Download `AnyPad-PS5-0.5.28-beta.elf` from [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases)
   and run it with your payload loader.
2. Open **AnyPad** in the console's *Media* row (or `http://<console-ip>:8095` from a phone or PC).
3. Press **Pair**, then put the controller in pairing mode (DualShock 4: Share + PS).
4. When the console asks who uses the controller, choose **Quick play / Guest** (a temporary user).

Use **Stop AnyPad** on that page before running it again.

### Known issues

- **DualShock 4:** pair it again each time you switch it on. Other controllers reconnect by themselves.
- **Keep your DualSense on the USB cable** while AnyPad runs (*Settings → Accessories → Controllers →
  Communication method → Use USB cable*). Otherwise, if it reconnects over Bluetooth, it can stay blinking blue.
- **If no controller is found:** with the DualSense on the cable, turn Bluetooth off and on again in the
  console settings (the console restarts), then run AnyPad again.
- If one controller is no longer found: remove it from *Settings → Accessories → Bluetooth accessories*,
  reset it (DualShock 4: the hole on the back, 5 s) and pair it again.

Controllers: see [COMPATIBILITY.md](COMPATIBILITY.md). Licence: GPL-3.0-or-later ([LICENSE](LICENSE)).
Author: **elmonomalvad0**, the same author as *sinfiltros* (the original AnyPad PS5 on GitHub).
X / Twitter: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contact: exposed.ct.ws@gmail.com

---

<a name="español"></a>
## 🇪🇸 Español

Usa otros mandos Bluetooth en una PS5 con jailbreak (DualShock 4, 8BitDo, Switch Pro y más), sin
adaptadores. La consola y los juegos los ven como un DualSense.

**Versión 0.5.28-beta (6 de octubre de 2026).** Probada en una PS5 fat, firmware 10.01, con un DualShock 4 y
un 8BitDo SF30 Pro (modo Switch).

### Cómo usarlo

1. Descarga `AnyPad-PS5-0.5.28-beta.elf` desde [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases)
   y ejecútalo con tu cargador de payloads.
2. Abre **AnyPad** en *Contenido multimedia* de la consola (o `http://<ip-de-la-consola>:8095` desde el móvil o el PC).
3. Pulsa **Emparejar** y pon el mando en modo emparejamiento (DualShock 4: Share + PS).
4. Cuando la consola pregunte quién usa el mando, elige **Juego rápido / Invitado** (un usuario temporal).

Usa **Apagar AnyPad** en esa página antes de volver a ejecutarlo.

### Problemas conocidos

- **DualShock 4:** hay que emparejarlo cada vez que lo enciendes. Los demás mandos se reconectan solos.
- **Deja el DualSense por cable USB** mientras uses AnyPad (*Ajustes → Accesorios → Mandos → Método de
  comunicación → Usar cable USB*). Si no, al reconectarse por Bluetooth puede quedarse parpadeando en azul.
- **Si no detecta ningún mando:** con el DualSense por cable, desactiva y vuelve a activar el Bluetooth en los
  Ajustes de la consola (se reinicia) y ejecuta AnyPad de nuevo.
- Si un mando concreto deja de aparecer: quítalo de *Ajustes → Accesorios → Accesorios Bluetooth*, resetéalo
  (DualShock 4: agujero trasero, 5 s) y vuelve a emparejarlo.

Mandos: ver [COMPATIBILITY.md](COMPATIBILITY.md). Licencia: GPL-3.0-or-later ([LICENSE](LICENSE)).
Autor: **elmonomalvad0**, el mismo autor que *sinfiltros* (el AnyPad PS5 original en GitHub).
X / Twitter: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contacto: exposed.ct.ws@gmail.com
