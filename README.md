<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS4"></p>

# AnyPad PS4

**Mandos de otras plataformas en una PS4 con jailbreak, por USB o desde el móvil por la red.**
**Controllers from other platforms on a jailbroken PS4, by USB or from a phone over the network.**

**[Español](#español) · [English](#english)**

AnyPad PS4 es un port, exclusivo para PS4, de [AnyPad PS5](https://codeberg.org/elmonomalva0/Any-Pad-ps5) de **elmonomalvad0**.
AnyPad PS4 is a PS4-only port of [AnyPad PS5](https://codeberg.org/elmonomalva0/Any-Pad-ps5) by **elmonomalvad0**.

---

<a name="español"></a>
# Español

AnyPad PS4 es un payload que hace que la consola vea mandos de otras marcas como **DualShock 4 virtuales**: en los menús, en los juegos y en los emuladores. Hay dos formas de conectarlos:

- **Por USB**, en los puertos de la consola.
- **Por la red local**: un móvil o un PC abre una página, su navegador lee el mando conectado a *ese* dispositivo (por cable o emparejado por **Bluetooth** con el móvil) y lo envía a la consola. Así se juega sin cable sin usar el Bluetooth de la consola.

Probado en una PS4 con firmware 13.52.

## Qué funciona y qué no

| | Estado |
| --- | --- |
| Mandos **Xbox 360** (XInput) por USB | ✅ probado (GameSir G8 Plus en modo Xbox 360) |
| Mandos **Xbox One** (GIP) por USB | ⚠️ implementado, sin probar todavía en PS4 |
| Mandos **HID** por USB con perfil (Switch Pro y compatibles, GameSir…) | ✅ probado |
| Cualquier otro mando **HID** por USB (perfil genérico, leyendo su descriptor) | ⚠️ implementado, sin probar todavía en PS4 |
| **GameSir Kaleid Flux** por USB | ✅ probado |
| **GameSir G8 Plus** por USB, en su modo Xbox 360 o Switch | ✅ probado |
| **Mando por red** desde el navegador de un móvil o PC | ✅ probado con un móvil real (GameSir y DualSense) |
| Mandos de **Sony** (DualShock 4, DualSense) por USB a través de AnyPad | ❌ la consola se los reserva (y ya funcionan de forma nativa) |
| Mandos de **Sony** por el mando por red | ✅ probado (DualSense) |
| Mandos por el **Bluetooth de la propia consola** | ❌ el sistema es el dueño de esa radio. Para jugar sin cable, usa el mando por red |

La lista por mando está en [COMPATIBILITY.md](COMPATIBILITY.md).

## Uso

1. **Carga el ELF** (`AnyPad-PS4-<versión>.elf`) con el cargador de payloads que uses normalmente.
2. Aparece una notificación con la dirección del **menú** (`http://<ip>:8095/`) y la del **mando por red** (`http://<ip>:8096/`). AnyPad también se añade a la fila de multimedia para abrir el menú desde la consola.
3. **Por USB**: conecta el mando. En cuanto envía su primer dato, la consola recibe un DualShock 4 virtual.
4. **Por red**: abre `http://<ip>:8096/` en el navegador del móvil o del PC (misma red), conecta allí el mando y pulsa un botón. Toca la pantalla una vez: la página la mantiene encendida mientras juegas. Baja el brillo para ahorrar batería. Puedes conectar **varios mandos al mismo móvil**: cada uno llega a la consola como un mando distinto.
5. Cuando la consola pregunte **quién usa el mando**, elige tu usuario. Si el mando no tiene botón PS, usa **Pulsar PS** en el menú.

Hasta 4 mandos a la vez: los USB ocupan los huecos desde arriba y los de red desde abajo.

### El menú

`http://<ip>:8095/`: los mandos conectados con sus botones, sticks y gatillos en vivo; la dirección del mando por red y cuántos dispositivos lo usan; **Pulsar PS** por mando; el log; y **Apagar AnyPad**.

**Apaga AnyPad antes de volver a cargar el ELF** (botón del menú, o crea el fichero vacío `/data/anypad/stop`). Al entrar la consola en reposo, AnyPad se para solo: vuelve a cargarlo al despertar.

### Ficheros en la consola

| Fichero | Para qué |
| --- | --- |
| `/data/anypad/anypad.log` | El log (y `.log.1`, el anterior) |
| `/data/anypad/maps/VVVV_PPPP.map` | Corrige botones o ejes de un mando sin perfil (ver [maps/EXAMPLE.map](maps/EXAMPLE.map)) |
| `/data/anypad/stop` | Vacío: para AnyPad |
| `/data/anypad/no_icon`, `/data/anypad/remove_icon` | No añadir el icono a la fila de multimedia / quitarlo una vez |

### Seguridad

El menú y el mando por red solo aceptan conexiones de la red local y no tienen contraseña: cualquiera en tu red puede abrirlos mientras AnyPad está en marcha. No expongas los puertos 8095 y 8096 a Internet.

## Compilar

```
make test                                   # tests en el ordenador
make ps4 PS4_PAYLOAD_SDK=/ruta/al/sdk       # el ELF, en dist/
make send PS4_HOST=<ip> PS4_PORT=<puerto>   # enviarlo al cargador de payloads
```

Necesitas el [ps4-payload-sdk](https://github.com/ps4-payload-dev/sdk) (con clang y lld). Los stubs de enlace de `libSceUsbd` y `libSceMbus`, que el SDK no trae, están en `tools/sce_stubs/` y el build los compila solo. `tools/build-elf-ps4-docker.sh <sdk> <imagen>` compila dentro de un contenedor con clang, lld y make. `tools/live.sh` habla con un AnyPad en marcha (estado, log en vivo, parar); ver [AI-WORKFLOW.md](AI-WORKFLOW.md).

## Créditos

- **[AnyPad PS5](https://codeberg.org/elmonomalva0/Any-Pad-ps5)**, de **elmonomalvad0**: el proyecto del que nace este port (pad virtual, perfiles de mandos, menú). Su historial está en el proyecto original y en el historial de git de este repositorio.
- **[Control4Free](https://github.com/MoHadiShibli/Control4Free)** (MoHadiShibli): la idea del mando por red, con el navegador como mando.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): perfiles por fabricante y modelo, y el formato de los reports de muchos mandos.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) y **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: la secuencia del pad virtual y el formato de sus datos.
- **[NoSleep.js](https://github.com/richtr/NoSleep.js)** (Rich Tibbett, MIT): los vídeos que mantienen despierta la pantalla del móvil.

## Licencia

GPL-3.0-or-later (ver [LICENSE](LICENSE)). Si publicas tu propia versión o incluyes AnyPad en otro programa, conserva los avisos, distribuye los cambios con la misma licencia y, por favor, menciona a elmonomalvad0 y enlaza el proyecto original.

---

<a name="english"></a>
# English

AnyPad PS4 is a payload that makes the console see controllers from other brands as **virtual DualShock 4s**: in the menus, in games and in emulators. There are two ways to connect them:

- **By USB**, on the console's ports.
- **Over the local network**: a phone or PC opens a page, its browser reads the controller connected to *that* device (by cable, or paired to the phone by **Bluetooth**) and sends it to the console. That gives wireless play without the console's own Bluetooth.

Tested on a PS4 with firmware 13.52.

## What works and what does not

| | Status |
| --- | --- |
| **Xbox 360** controllers (XInput) by USB | ✅ tested (GameSir G8 Plus in Xbox 360 mode) |
| **Xbox One** controllers (GIP) by USB | ⚠️ implemented, not tested on a PS4 yet |
| **HID** controllers by USB with a profile (Switch Pro and compatibles, GameSir…) | ✅ tested |
| Any other **HID** controller by USB (generic profile, from its descriptor) | ⚠️ implemented, not tested on a PS4 yet |
| **GameSir Kaleid Flux** by USB | ✅ tested |
| **GameSir G8 Plus** by USB, in its Xbox 360 or Switch mode | ✅ tested |
| **Network controller** from a phone or PC browser | ✅ tested with a real phone (GameSir and DualSense) |
| **Sony** controllers (DualShock 4, DualSense) by USB through AnyPad | ❌ the console keeps them for itself (and they already work natively) |
| **Sony** controllers through the network controller | ✅ tested (DualSense) |
| Controllers over the **console's own Bluetooth** | ❌ the system owns that radio. For wireless play, use the network controller |

The list, controller by controller, is in [COMPATIBILITY.md](COMPATIBILITY.md).

## Use

1. **Load the ELF** (`AnyPad-PS4-<version>.elf`) with the payload loader you normally use.
2. A notification shows the address of the **menu** (`http://<ip>:8095/`) and of the **network controller** (`http://<ip>:8096/`). AnyPad also adds itself to the media row, to open the menu from the console.
3. **By USB**: plug the controller in. As soon as it sends its first report, the console gets a virtual DualShock 4.
4. **Over the network**: open `http://<ip>:8096/` in the browser of a phone or PC (same network), connect the controller there and press a button. Tap the screen once: the page keeps it on while you play. Turn the brightness down to save battery. You can connect **several controllers to the same phone**: each one reaches the console as a separate controller.
5. When the console asks **who is using the controller**, choose your user. If the controller has no PS button, use **Press PS** in the menu.

Up to 4 controllers at once: USB ones take the slots from the top, network ones from the bottom.

### The menu

`http://<ip>:8095/`: the connected controllers with their buttons, sticks and triggers live; the network controller's address and how many devices use it; **Press PS** per controller; the log; and **Stop AnyPad**.

**Stop AnyPad before loading the ELF again** (the menu's button, or create the empty file `/data/anypad/stop`). When the console goes to rest mode, AnyPad stops by itself: load it again after waking up.

### Files on the console

| File | What for |
| --- | --- |
| `/data/anypad/anypad.log` | The log (and `.log.1`, the previous one) |
| `/data/anypad/maps/VVVV_PPPP.map` | Fixes buttons or axes of a controller with no profile (see [maps/EXAMPLE.map](maps/EXAMPLE.map)) |
| `/data/anypad/stop` | Empty: stops AnyPad |
| `/data/anypad/no_icon`, `/data/anypad/remove_icon` | Do not add the icon to the media row / remove it once |

### Security

The menu and the network controller only accept connections from the local network and have no password: anyone on your network can open them while AnyPad runs. Do not expose ports 8095 and 8096 to the Internet.

## Building

```
make test                                   # tests on the computer
make ps4 PS4_PAYLOAD_SDK=/path/to/sdk       # the ELF, in dist/
make send PS4_HOST=<ip> PS4_PORT=<port>     # send it to the payload loader
```

You need the [ps4-payload-sdk](https://github.com/ps4-payload-dev/sdk) (with clang and lld). The link stubs for `libSceUsbd` and `libSceMbus`, which the SDK lacks, are in `tools/sce_stubs/` and the build makes them by itself. `tools/build-elf-ps4-docker.sh <sdk> <image>` builds inside a container with clang, lld and make. `tools/live.sh` talks to a running AnyPad (state, live log, stop); see [AI-WORKFLOW.md](AI-WORKFLOW.md).

## Credits

- **[AnyPad PS5](https://codeberg.org/elmonomalva0/Any-Pad-ps5)**, by **elmonomalvad0**: the project this port comes from (virtual pad, controller profiles, menu). Its history is in the original project and in this repository's git history.
- **[Control4Free](https://github.com/MoHadiShibli/Control4Free)** (MoHadiShibli): the idea of the network controller, with the browser as the pad.
- **[MissionControl](https://github.com/ndeadly/MissionControl)** (ndeadly): profiles by vendor and model, and the report layouts of many controllers.
- **[SplashDown](https://github.com/seregonwar/SplashDown)** (seregonwar) and **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: the virtual pad sequence and its data layout.
- **[NoSleep.js](https://github.com/richtr/NoSleep.js)** (Rich Tibbett, MIT): the clips that keep the phone's screen awake.

## License

GPL-3.0-or-later (see [LICENSE](LICENSE)). If you publish your own version or include AnyPad in another program, keep the notices, distribute your changes under the same license and, please, mention elmonomalvad0 and link the original project.
