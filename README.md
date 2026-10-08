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

**Version 0.7.0-beta (7 October 2026).** Tested on a PS5 fat, firmware 10.01, with a DualShock 4 and an
8BitDo SF30 Pro (Switch mode). **Tested in the console menus, in games and in emulators: you can use it with
confidence.**

What's new: **the system has been reinforced.** Pairing is much faster and more reliable (the DualShock 4 pairs
in seconds), and AnyPad no longer sends the controller chip a wrong command on every run. If a controller you
switched on earlier has called the console, the **Pair** button is disabled and **tells you in its own text how
many seconds are left** before the 60-second search starts; the message disappears when the search begins.
Every run starts clean: nothing about your controllers is kept, so you pair them each time.

### How to use it

1. Download `AnyPad-PS5-0.7.0-beta.elf` from [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases)
   and run it with your payload loader.
2. Open **AnyPad** in the console's *Media* row (or `http://<console-ip>:8095` from a phone or PC).
3. Press **Pair**, then put the controller in pairing mode (DualShock 4: Share + PS).
4. When the console asks who uses the controller, choose **Quick play / Guest** (a temporary user).

When you finish, **always exit with the *Stop AnyPad PS5* button** on that page (also before running it again).

### If no controller is found

1. **Turn Bluetooth off and on again** in the console settings (the console restarts), then run AnyPad again.
2. **Reset the controller if it has been paired before.** A controller that remembers a previous pairing may
   not show up. Examples:
   - **DualShock 4:** with a paper clip, press the tiny reset button inside the small hole on the back of the
     controller, next to the L2 button, for about 5 seconds. Then put it in pairing mode (Share + PS).
   - **8BitDo (Pro 2 and similar):** hold the **Pair** button for about 5 seconds until the light goes out
     (this restores factory settings and clears its pairings). Then hold **Pair** for about 3 seconds, until the
     lights blink fast, to put it in pairing mode. Some 8BitDo models have a reset hole instead.
   - **Any other controller:** search for how to reset *your* model.
3. Press **Pair** first and put the controller in pairing mode afterwards, close to the console.

### Known issues

- **DualSense: do not switch it off while AnyPad is running.** If the DualSense is forced off, it enters the
  blinking blue mode and does not work until AnyPad is stopped. We will improve this in a future version. If you
  do not switch it off, everything works great.
- **Pair your controllers each time you run AnyPad.** Nothing is kept between runs.

### Support the project

Many people ask me to test other controllers and other console versions. Any help is very welcome to **port this project to PS4** and to **PS5 firmware 13.60**, and to support **more controllers**. I would love to have a console on firmware 13.60 as well as my 10.01. It is not mandatory, but anyone who wants to help the project can do so on my Throne page:

👉 **[https://throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

### Help us confirm compatibility

If you can confirm that AnyPad PS5 works with any of the compatible controllers **that have not yet been tested**, please send us the log at [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com). This will help us a lot!

### Credits

AnyPad PS5 is original code: no code was copied from other projects. These projects were studied to
understand how things work, and their ideas were reinterpreted and written anew. Thanks to their authors:

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore): the idea behind the Bluetooth
  part, sharing the PS5's own Bluetooth chip with the system instead of taking it over.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): controller profiles by vendor
  and model, and the report layouts of many controllers.
- **Ghostcontrol** (StonedModder): how to create a virtual DualSense on the PS5 and assign it to a user
  (Ghostcontrol does it for USB controllers; AnyPad does it for Bluetooth ones).
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) and
  **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: the virtual pad sequence and the pad data layout.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom): the toolchain everything
  is built with.

Controllers: see [COMPATIBILITY.md](COMPATIBILITY.md). Licence: GPL-3.0-or-later ([LICENSE](LICENSE)).
Author: **elmonomalvad0**, the same author as *sinfiltros* (the original AnyPad PS5 on GitHub).
X / Twitter: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contact: exposed.ct.ws@gmail.com

---

<a name="español"></a>
## 🇪🇸 Español

Usa otros mandos Bluetooth en una PS5 con jailbreak (DualShock 4, 8BitDo, Switch Pro y más), sin
adaptadores. La consola y los juegos los ven como un DualSense.

**Versión 0.7.0-beta (7 de octubre de 2026).** Probada en una PS5 fat, firmware 10.01, con un DualShock 4 y
un 8BitDo SF30 Pro (modo Switch). **Probada en los menús de la consola, en juegos y en emuladores: puedes usarla
con confianza.**

Novedades: **el sistema se ha reforzado.** El emparejado es mucho más rápido y fiable (el DualShock 4 se empareja
en segundos), y AnyPad ya no envía una orden errónea al chip de los mandos en cada ejecución. Si un mando que has
encendido antes ha llamado a la consola, el botón **Emparejar** queda deshabilitado y **te dice en su propio texto
cuántos segundos faltan** para que empiece la búsqueda de 60 segundos; el mensaje desaparece cuando empieza la
búsqueda. Cada ejecución empieza limpia: no se guarda nada de tus mandos, así que los emparejas cada vez.

### Cómo usarlo

1. Descarga `AnyPad-PS5-0.7.0-beta.elf` desde [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases)
   y ejecútalo con tu cargador de payloads.
2. Abre **AnyPad** en *Contenido multimedia* de la consola (o `http://<ip-de-la-consola>:8095` desde el móvil o el PC).
3. Pulsa **Emparejar** y pon el mando en modo emparejamiento (DualShock 4: Share + PS).
4. Cuando la consola pregunte quién usa el mando, elige **Juego rápido / Invitado** (un usuario temporal).

Al terminar, **sal siempre con el botón *Apagar AnyPad PS5*** de esa página (también antes de volver a ejecutarlo).

### Si no detecta ningún mando

1. **Desactiva y vuelve a activar el Bluetooth** en los Ajustes de la consola (se reinicia) y ejecuta AnyPad de nuevo.
2. **Haz un reset del mando si ya ha estado emparejado.** Un mando que recuerda un emparejado anterior puede no
   aparecer. Ejemplos:
   - **DualShock 4:** con un clip, pulsa el pequeño botón de reset del agujero de la parte trasera del mando,
     junto al botón L2, durante unos 5 segundos. Después ponlo en modo emparejamiento (Share + PS).
   - **8BitDo (Pro 2 y parecidos):** mantén el botón **Pair** unos 5 segundos hasta que se apague la luz (restaura
     los ajustes de fábrica y borra sus emparejamientos). Después mantén **Pair** unos 3 segundos, hasta que las
     luces parpadeen rápido, para ponerlo en modo emparejamiento. Algunos modelos de 8BitDo tienen un agujero de
     reset en su lugar.
   - **Cualquier otro mando:** busca cómo se hace el reset de *tu* modelo.
3. Pulsa **Emparejar** primero y pon el mando en modo emparejamiento después, cerca de la consola.

### Problemas conocidos

- **DualSense: no lo apagues mientras AnyPad está en marcha.** Si se fuerza el apagado del DualSense, entra en el
  modo azul parpadeante y no funciona hasta que se detenga AnyPad. Lo mejoraremos en una próxima versión. Si no lo
  apagas, todo funciona estupendamente.
- **Empareja tus mandos cada vez que ejecutes AnyPad.** No se guarda nada entre ejecuciones.

### Apoya el proyecto

Mucha gente me pide que pruebe otros mandos y otras versiones de la consola. Agradecemos cualquier ayuda para poder **portar este proyecto a PS4**, a las versiones **13.60 de PS5** y a **diferentes mandos**. Me encantaría tener una consola con el firmware 13.60 además de la mía, con el 10.01. No es obligatorio, pero quien quiera ayudar al proyecto puede hacerlo desde mi página de Throne:

👉 **[https://throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

### Ayúdanos a confirmar la compatibilidad

Si puedes confirmar que AnyPad PS5 funciona con alguno de los mandos compatibles **que aún no se han comprobado**, mándanos el log a [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com). ¡Esto nos ayudará mucho!

### Agradecimientos

AnyPad PS5 es código propio: no se ha copiado código de otros proyectos. Estos proyectos se estudiaron para
entender cómo funcionan las cosas, y sus ideas se reinterpretaron y se escribieron de nuevo. Gracias a sus autores:

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)** (FGGstore): la idea de la parte Bluetooth,
  compartir el propio chip Bluetooth de la PS5 con el sistema en lugar de quitárselo.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): los perfiles de mandos por
  fabricante y modelo, y el formato de los informes de muchos mandos.
- **Ghostcontrol** (StonedModder): cómo crear un DualSense virtual en la PS5 y asignarlo a un usuario
  (Ghostcontrol lo hace con mandos USB; AnyPad, con mandos Bluetooth).
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) y
  **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: la secuencia del mando virtual y el formato de los
  datos del mando.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** (John Törnblom): las herramientas con las que
  se compila todo.

Mandos: ver [COMPATIBILITY.md](COMPATIBILITY.md). Licencia: GPL-3.0-or-later ([LICENSE](LICENSE)).
Autor: **elmonomalvad0**, el mismo autor que *sinfiltros* (el AnyPad PS5 original en GitHub).
X / Twitter: [@elmonomalvad0](https://x.com/elmonomalvad0) · Contacto: exposed.ct.ws@gmail.com
