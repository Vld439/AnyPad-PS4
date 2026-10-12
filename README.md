<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS5"></p>

# AnyPad PS5: Free your PS5 from the DualSense

**A Bluetooth controller bridge for jailbroken PS5 consoles.**
**Un puente de mandos Bluetooth para PS5 con jailbreak.**

**[English](#english) · [Español](#español)** · [Français](README.fr.md) · [Italiano](README.it.md) · [Português](README.pt.md) · [Deutsch](README.de.md) · [Русский](README.ru.md) · [简体中文](README.zh.md) · [繁體中文](README.zh-Hant.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md)

---

<a name="english"></a>
# English

AnyPad PS5 lets you use controllers from other platforms on your PS5. It connects them through the console's own Bluetooth chip and presents them to the system as virtual DualSense controllers.

No USB dongle required.

**Version 0.9.1-beta.** Tested on a PS5 fat, firmware 10.01, with a DualShock 4 and an 8BitDo SF30 Pro (Switch mode), in the console menus, in games and in emulators.

**Available in twelve languages:** English, Spanish, French, Italian, Portuguese, German, Russian, Simplified Chinese, Traditional Chinese, Japanese, Korean and Arabic. The AnyPad page follows the console language.

---

## About this project

AnyPad PS5 started on 3 October 2026 at about 14:20 (Spanish time), the day I published its first version. The idea came from MissionControl, which lets the Nintendo Switch use controllers from other consoles. AnyPad may not have been the first project to use other controllers on the PS5: there were already projects that connected them by USB. But none connected them by Bluetooth, and AnyPad was the first to do it. I had an idea and I carried it out.

The goal now is to make AnyPad work with more controllers. If you try it, please send me your log (see "Help us confirm compatibility" below): every log helps.

If you develop an emulator and would like to integrate AnyPad, I am willing to share the code with you. Write to me at [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com).

One known issue remains: if the DualSense is switched off while AnyPad is running, it can stay blinking blue. I hope future projects do not have it. Projects forked from AnyPad, from the code I published, will very probably inherit this issue. It is part of AnyPad's legacy, and a sign that, at least in their Bluetooth part, they are forks of AnyPad.

---

## What is new in 0.9

- **More compatible:** more controllers supported, and support for the two Bluetooth chips used in PS5 consoles.
- **Bug fixes.**
- **New graphical interface** for the AnyPad page.

## What is new in 0.8

### Faster pairing

Pairing is now much faster and more reliable. In our tests the DualShock 4 and the 8BitDo are found in a few seconds and are ready a few seconds later, where earlier versions often needed several attempts.

### More stable

- **Smooth response:** controllers react fluidly while you play, without lag.
- **Controllers switched on normally** (instead of in pairing mode) are now paired at the first attempt.
- **Smarter handling** of controllers that are slow to answer or refuse a step.

### Configure PS

For controllers without a usable PS button (Switch Pro, 8BitDo and others), the **Configure PS** button lets you choose **one button or a combination of three** that will act as the PS button. Choose how many, press them on the controller and hold them for one second: AnyPad detects them by itself. The setting is saved for that controller model.

DualShock 4 and DualSense controllers have their own PS button, so this option does not appear for them.

### Twelve languages

The AnyPad page follows the console language: English, Spanish, French, Italian, Portuguese, German, Russian, Simplified Chinese, Traditional Chinese, Japanese, Korean and Arabic (right to left). Any other language falls back to English.

---

## ⚠️ Important: do not switch off the DualSense while AnyPad is running

If the DualSense is switched off while AnyPad is running, it can enter the blinking blue mode and stop working until the console is restarted.

So that the console does not switch it off by itself after a while without use, set:

**Settings → System → Power Saving → Set Time Until Controllers Turn Off → Don't Turn Off**

If you do not switch it off, everything works normally.

---

## ⚠️ Important: choose "Quick Play / Guest" for the external controller

When you press PS on an external controller, the console asks who will use it. Choose **Quick Play / Guest**. Your main user keeps the DualSense, and you can go back to it at any time by picking up the DualSense.

---

# Controller compatibility

| Controller | Status | Notes |
| --- | --- | --- |
| **DualShock 4** | ✅ **Verified** | Put it in pairing mode (Share + PS) every time you pair it. |
| **8BitDo SF30 Pro (Switch mode)** | ✅ **Verified** | It shows up as a Switch Pro Controller. Use Configure PS for the PS button. |
| **GameSir G7 SE** | ✅ **Verified** | |
| **Nintendo Switch Pro Controller** | 🧪 **Supported, not tested yet** | Same protocol as the 8BitDo in Switch mode. |
| **Xbox controllers** | ✅ **Working (reported by users)** | Users report them working since 0.8. If yours does not connect, please send me your log and its model. |
| **Any other Bluetooth HID gamepad** | 🧪 **Generic profile** | May work without a specific profile. |

**Controllers we believe are compatible** (supported in the code, not tested on a console yet): DualShock 4 v1, Hori Onyx, Razer Raiju Ultimate, Razer Raiju Tournament Edition, SCUF Vantage 2, Nintendo Switch Pro Controller, Nintendo Switch Online (SNES, N64, Mega Drive / Genesis), GameSir G3s, G4s, G7 Pro, T1s and T2a, Betop 2585N2, Bionik Vulkan, LanShen X1Pro, Xiaomi Mi Controller, Amazon Fire / iPega, iPega 9-series, 8BitDo SN30 Pro for Xbox Cloud Gaming, 8BitDo Ultimate 2.4G, SteelSeries, NVIDIA Shield controller (2017), Razer Serval, PowerA MOGA Hero, MOGA Pro and MOGA Pro 2, Atari VCS Modern controller, Hyperkin Scout, Mocute 050. If you try one of them, please send me your log.

The full list, controller by controller, is in [COMPATIBILITY.md](COMPATIBILITY.md).

---

# How it works

AnyPad PS5 runs as a resident payload on a jailbroken PS5. It uses the console's own Bluetooth chip, **sharing it with the system**, to discover and pair controllers, read their input and present them to the PS5 as virtual DualSense controllers.

It does **not** modify games, the PS5 firmware or the jailbreak. Your PS5 must already be able to run payloads.

---

# How to use

1. **Load AnyPad.** Download `AnyPad-PS5-0.9.1-beta.elf` from [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases) and load it with the payload loader you normally use.
2. **Open the page.** Open **AnyPad** in the console's Media row, or open `http://<console-ip>:8095` from a phone or computer on the same network.
3. **Pair.** Press **Pair**, then put the controller in pairing mode, close to the console.

| Controller | Pairing mode |
| --- | --- |
| **DualShock 4** | Hold **Share + PS** until the light bar flashes twice |
| **8BitDo (Switch mode)** | Hold **Pair** for about 3 seconds, until the lights blink fast |
| **Nintendo Switch Pro** | Press the small sync button next to the USB-C port |
| **Other controllers** | Follow the manufacturer's instructions |

4. **Choose the user.** When the console asks, choose **Quick Play / Guest**.

Several controllers can be used at the same time, together with the DualSense.

---

# Every run starts clean

AnyPad does not keep paired controllers between runs. Pair your controllers each time you start AnyPad. This avoids old pairings that no longer work and keeps the Bluetooth chip in a known state.

If a controller that was switched off reconnects by itself and does not respond, switch it off and pair it again from the page.

---

# Stopping AnyPad

Always stop it with **Stop AnyPad PS5** on its page, also before running it again and before switching off or restarting the console. The **Exit** button only closes the page.

---

# Troubleshooting

### No controller is found

1. Press **Pair** first and put the controller in pairing mode afterwards.
2. Make sure the controller really is in pairing mode (for example, the DualShock 4 light bar must flash twice).
3. If the controller was paired with another device before and still does not show up, reset it (DualShock 4: a paper clip in the small hole next to L2 for about 5 seconds; 8BitDo Pro 2: hold Pair for about 5 seconds) and try again.

### Another controller payload is running

Another payload that uses the Bluetooth chip and port 8095 gets in the way. Stop it from its own menu before running AnyPad; AnyPad shows a notice when it detects this.

### The page does not open

Check that AnyPad is running, that your phone or computer is on the same network as the PS5, and that you use the console's IP address with port `8095`.

---

# Help us confirm compatibility

If you can confirm that AnyPad PS5 works (or does not work) with a controller, please send the log (`/data/anypad/anypad.log`) to [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com), with the controller model, the console firmware, the jailbreak and the AnyPad version. This helps a lot.

### Support the project

Many people ask me to test other controllers and other console versions. Any help is very welcome to **port this project to PS4** and to **PS5 firmware 13.60**, and to support **more controllers**. I would love to have a console on firmware 13.60 as well as my 10.01. It is not mandatory, but anyone who wants to help the project can do so on my Throne page: **[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# Credits

AnyPad PS5 is original code. These projects were studied to understand how things work, and their ideas were written anew for AnyPad. Thanks to their authors:

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore): the idea behind the Bluetooth part, sharing the PS5's own Bluetooth chip with the system.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): controller profiles by vendor and model, and the report layouts of many controllers.
- **Ghostcontrol** (StonedModder): how to create a virtual DualSense on the PS5 and assign it to a user.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) and **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: the virtual pad sequence and the pad data layout.
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)** (X-F1REBALL-X): ideas on handling Bluetooth negotiation.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom): the toolchain everything is built with.

License: GPL-3.0-or-later ([LICENSE](LICENSE)).

Author: **elmonomalvad0**, the same author as *sinfiltros* (the original AnyPad PS5 on GitHub).
X: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contact: exposed.ct.ws@gmail.com

---

<a name="español"></a>
# Español

AnyPad PS5 te permite usar en tu PS5 mandos de otras plataformas. Los conecta a través del propio chip Bluetooth de la consola y los presenta al sistema como DualSense virtuales.

No hace falta ningún adaptador USB.

**Versión 0.9.1-beta.** Probada en una PS5 fat, firmware 10.01, con un DualShock 4 y un 8BitDo SF30 Pro (modo Switch), en los menús de la consola, en juegos y en emuladores.

**Disponible en doce idiomas:** español, inglés, francés, italiano, portugués, alemán, ruso, chino simplificado, chino tradicional, japonés, coreano y árabe. La página de AnyPad sigue el idioma de la consola.

---

## Sobre este proyecto

AnyPad PS5 empezó el 3 de octubre de 2026, hacia las 14:20 (hora de España), el día que publiqué su primera versión. La idea partió de MissionControl, que permite usar en la Nintendo Switch mandos de otras consolas. Quizá AnyPad no fue el primer proyecto para usar otros mandos en la PS5: ya había proyectos que los conectaban por USB. Pero ninguno los conectaba por Bluetooth, y AnyPad fue el primero en hacerlo. Tuve una idea y la llevé a cabo.

El objetivo ahora es que AnyPad funcione con más mandos. Si lo pruebas, por favor envíame tu log (mira «Ayúdanos a confirmar la compatibilidad» más abajo): cada log ayuda.

Si desarrollas un emulador y quieres integrar AnyPad, estoy dispuesto a compartir el código contigo. Escríbeme a [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com).

Queda un fallo conocido: si se apaga el DualSense con AnyPad en marcha, puede quedarse en azul parpadeante. Espero que los próximos proyectos no lo tengan. Los que sean forks de AnyPad, del código que publiqué, muy probablemente heredarán ese fallo. Es parte del legado de AnyPad, y una muestra de que, al menos en su parte Bluetooth, son un fork de AnyPad.

---

## Novedades de la 0.9

- **Más compatible:** admite más mandos y es compatible con los dos chips Bluetooth que llevan las PS5.
- **Errores corregidos.**
- **Nuevo entorno gráfico** en la página de AnyPad.

## Novedades de la 0.8

### Emparejado más rápido

El emparejado es ahora mucho más rápido y fiable. En nuestras pruebas, el DualShock 4 y el 8BitDo aparecen en pocos segundos y quedan listos pocos segundos después, cuando antes a menudo hacían falta varios intentos.

### Más estable

- **Respuesta fluida:** los mandos reaccionan con suavidad mientras juegas, sin lag.
- **Mandos encendidos de forma normal** (en vez de en modo emparejamiento): ahora se emparejan al primer intento.
- **Tratamiento más inteligente** de los mandos que tardan en responder o rechazan un paso.

### Configurar PS

Para los mandos sin un botón PS que funcione (Switch Pro, 8BitDo y otros), el botón **Configurar PS** permite elegir **un botón o una combinación de tres** que hará de botón PS. Eliges cuántos, los pulsas en el mando y los mantienes un segundo: AnyPad los detecta solo. El ajuste se guarda para ese modelo de mando.

Los DualShock 4 y los DualSense tienen su propio botón PS, así que esta opción no aparece para ellos.

### Doce idiomas

La página de AnyPad sigue el idioma de la consola: español, inglés, francés, italiano, portugués, alemán, ruso, chino simplificado, chino tradicional, japonés, coreano y árabe (de derecha a izquierda). Cualquier otro idioma usa el inglés.

---

## ⚠️ Importante: no apagues el DualSense con AnyPad en marcha

Si el DualSense se apaga mientras AnyPad está en marcha, puede entrar en el modo azul parpadeante y no funcionar hasta reiniciar la consola.

Para que la consola no lo apague sola tras un rato sin usarlo, configura:

**Ajustes → Sistema → Ahorro de energía → Establecer tiempo hasta que se apaguen los mandos → No apagar**

Si no lo apagas, todo funciona con normalidad.

---

## ⚠️ Importante: elige «Juego rápido / Invitado» para el mando externo

Cuando pulsas PS en un mando externo, la consola pregunta quién lo va a usar. Elige **Juego rápido / Invitado**. Tu usuario principal conserva el DualSense, y puedes volver a él en cualquier momento cogiendo el DualSense.

---

# Compatibilidad de mandos

| Mando | Estado | Notas |
| --- | --- | --- |
| **DualShock 4** | ✅ **Verificado** | Ponlo en modo emparejamiento (Share + PS) cada vez que lo emparejes. |
| **8BitDo SF30 Pro (modo Switch)** | ✅ **Verificado** | Aparece como Switch Pro Controller. Usa Configurar PS para el botón PS. |
| **GameSir G7 SE** | ✅ **Verificado** | |
| **Nintendo Switch Pro Controller** | 🧪 **Soportado, sin probar** | Mismo protocolo que el 8BitDo en modo Switch. |
| **Mandos Xbox** | ✅ **Funcionan (según usuarios)** | Varios usuarios confirman que funcionan desde la 0.8. Si el tuyo no conecta, envíame tu log y el modelo. |
| **Cualquier otro mando HID Bluetooth** | 🧪 **Perfil genérico** | Puede funcionar sin un perfil específico. |

**Mandos que creemos compatibles** (soportados en el código, aún sin probar en una consola): DualShock 4 v1, Hori Onyx, Razer Raiju Ultimate, Razer Raiju Tournament Edition, SCUF Vantage 2, Nintendo Switch Pro Controller, Nintendo Switch Online (SNES, N64, Mega Drive / Genesis), GameSir G3s, G4s, G7 Pro, T1s y T2a, Betop 2585N2, Bionik Vulkan, LanShen X1Pro, Xiaomi Mi Controller, Amazon Fire / iPega, iPega 9-series, 8BitDo SN30 Pro para Xbox Cloud Gaming, 8BitDo Ultimate 2.4G, SteelSeries, mando NVIDIA Shield (2017), Razer Serval, PowerA MOGA Hero, MOGA Pro y MOGA Pro 2, mando Atari VCS Modern, Hyperkin Scout, Mocute 050. Si pruebas alguno, envíame tu log.

La lista completa, mando por mando, está en [COMPATIBILITY.md](COMPATIBILITY.md).

---

# Cómo funciona

AnyPad PS5 se ejecuta como un payload residente en una PS5 con jailbreak. Usa el propio chip Bluetooth de la consola, **compartiéndolo con el sistema**, para buscar y emparejar mandos, leer sus botones y presentarlos a la PS5 como DualSense virtuales.

**No** modifica juegos, el firmware de la PS5 ni el jailbreak. Tu PS5 ya debe poder ejecutar payloads.

---

# Cómo usarlo

1. **Carga AnyPad.** Descarga `AnyPad-PS5-0.9.1-beta.elf` desde [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases) y cárgalo con el cargador de payloads que uses normalmente.
2. **Abre la página.** Abre **AnyPad** en *Contenido multimedia* de la consola, o `http://<ip-de-la-consola>:8095` desde un móvil o un PC de la misma red.
3. **Empareja.** Pulsa **Emparejar** y después pon el mando en modo emparejamiento, cerca de la consola.

| Mando | Modo emparejamiento |
| --- | --- |
| **DualShock 4** | Mantén **Share + PS** hasta que la barra de luz haga un doble destello |
| **8BitDo (modo Switch)** | Mantén **Pair** unos 3 segundos, hasta que las luces parpadeen rápido |
| **Nintendo Switch Pro** | Pulsa el pequeño botón de sincronización junto al puerto USB-C |
| **Otros mandos** | Sigue las instrucciones del fabricante |

4. **Elige el usuario.** Cuando la consola lo pregunte, elige **Juego rápido / Invitado**.

Se pueden usar varios mandos a la vez, junto con el DualSense.

---

# Cada ejecución empieza limpia

AnyPad no guarda mandos emparejados entre ejecuciones. Empareja tus mandos cada vez que arranques AnyPad. Así se evitan emparejados antiguos que ya no funcionan y el chip Bluetooth queda en un estado conocido.

Si un mando que se apagó se reconecta solo y no responde, apágalo y vuelve a emparejarlo desde la página.

---

# Apagar AnyPad

Páralo siempre con **Apagar AnyPad PS5** en su página, también antes de volver a ejecutarlo y antes de apagar o reiniciar la consola. El botón **Salir** solo cierra la página.

---

# Solución de problemas

### No encuentra ningún mando

1. Pulsa **Emparejar** primero y pon el mando en modo emparejamiento después.
2. Comprueba que el mando está de verdad en modo emparejamiento (por ejemplo, la barra de luz del DualShock 4 debe hacer un doble destello).
3. Si el mando estuvo emparejado antes con otro aparato y sigue sin aparecer, haz un reset (DualShock 4: un clip en el agujero junto a L2 unos 5 segundos; 8BitDo Pro 2: mantén Pair unos 5 segundos) e inténtalo de nuevo.

### Hay otro payload de mandos en marcha

Otro payload que use el chip Bluetooth y el puerto 8095 interfiere. Páralo desde su propio menú antes de ejecutar AnyPad; AnyPad avisa en pantalla cuando lo detecta.

### La página no se abre

Comprueba que AnyPad está en marcha, que tu móvil o PC está en la misma red que la PS5 y que usas la IP de la consola con el puerto `8095`.

---

# Ayúdanos a confirmar la compatibilidad

Si puedes confirmar que AnyPad PS5 funciona (o no funciona) con un mando, envía el log (`/data/anypad/anypad.log`) a [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com), con el modelo del mando, el firmware de la consola, el jailbreak y la versión de AnyPad. Esto ayuda mucho.

### Apoya el proyecto

Mucha gente me pide que pruebe otros mandos y otras versiones de la consola. Agradecemos cualquier ayuda para poder **portar este proyecto a PS4**, a las versiones **13.60 de PS5** y a **diferentes mandos**. Me encantaría tener una consola con el firmware 13.60 además de la mía, con el 10.01. No es obligatorio, pero quien quiera ayudar al proyecto puede hacerlo desde mi página de Throne: **[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# Agradecimientos

AnyPad PS5 es código propio. Estos proyectos se estudiaron para entender cómo funcionan las cosas, y sus ideas se escribieron de nuevo para AnyPad. Gracias a sus autores:

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore): la idea de la parte Bluetooth, compartir el propio chip Bluetooth de la PS5 con el sistema.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): los perfiles de mandos por fabricante y modelo, y el formato de los informes de muchos mandos.
- **Ghostcontrol** (StonedModder): cómo crear un DualSense virtual en la PS5 y asignarlo a un usuario.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) y **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: la secuencia del mando virtual y el formato de los datos del mando.
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)** (X-F1REBALL-X): ideas sobre la negociación Bluetooth.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom): las herramientas con las que se compila todo.

Licencia: GPL-3.0-or-later ([LICENSE](LICENSE)).

Autor: **elmonomalvad0**, el mismo autor que *sinfiltros* (el AnyPad PS5 original en GitHub).
X: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contacto: exposed.ct.ws@gmail.com
