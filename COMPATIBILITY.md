# Compatibility / Compatibilidad — AnyPad PS4

Tested on a PS4 with firmware 13.52. ✅ tested · ⚠️ implemented, not tested on a PS4 yet · ❌ not possible.
Probado en una PS4 con firmware 13.52. ✅ probado · ⚠️ implementado, sin probar en PS4 · ❌ no es posible.

## By USB / Por USB

| Controller / Mando | Id | How AnyPad reads it / Cómo lo lee | Status / Estado |
| --- | --- | --- | --- |
| GameSir Kaleid Flux | `3537:1097` | HID, GameSir profile (report `0x05`) | ✅ |
| GameSir G8 Plus, Xbox 360 mode | `3537:1014` | Xbox 360 (XInput) | ✅ |
| GameSir G8 Plus, Switch mode | `057e:2009` | HID, Switch Pro profile (USB handshake, then a keep-alive every 500 ms or it drops) | ✅ |
| Other Xbox 360 (XInput) controllers | class `ff/5d/01` | Xbox 360 (XInput) | ⚠️ |
| Xbox One / Series (GIP) controllers | class `ff/47/d0` | GIP, powered on by AnyPad | ⚠️ |
| Other HID controllers with a profile (Switch Pro, DualShock 4 clones by other brands…) | see `src/profiles.c` | their profile | ⚠️ |
| Any other HID gamepad | — | generic profile, from its report descriptor; buttons and axes can be fixed with `/data/anypad/maps/VVVV_PPPP.map` | ⚠️ |
| Sony DualShock 3 (PS3) | `054c:0268` | HID, DualShock 3 profile (woken with its feature report `0xF2`; LEDs by SET_REPORT) | ⚠️ |
| Sony DualShock 4, DualSense | `054c:…` | left to the console: console kernel refuses USB endpoints (0x802400ff); use network controller via phone/PC / el kernel bloquea endpoints por USB; usar por red vía móvil/PC | ❌ |

The GameSir G8 Plus picks its USB mode by itself when plugged in; both modes work. Its own PS4 mode is a native console controller and does not go through AnyPad.
El GameSir G8 Plus elige solo su modo USB al conectarlo; los dos modos funcionan. Su modo PS4 propio es un mando nativo de la consola y no pasa por AnyPad.

A controller's headphone jack is handled by the console, not by AnyPad.
El conector de auriculares de un mando lo gestiona la consola, no AnyPad.

## Over the network / Por la red

The page at `http://<ip>:8096/` sends whatever controller the phone's or PC's browser reads (Gamepad API, standard mapping): plugged into that device, or paired to it by Bluetooth.
La página `http://<ip>:8096/` envía el mando que lea el navegador del móvil o del PC (Gamepad API, mapeo estándar): conectado a ese dispositivo o emparejado con él por Bluetooth.

| Controller / Mando | Status / Estado |
| --- | --- |
| GameSir, through a phone / desde un móvil | ✅ |
| DualSense, through a phone / desde un móvil | ✅ |

## Over the console's Bluetooth / Por el Bluetooth de la consola

✅ A DualSense or a Switch Pro already linked to the console, with the menu's option on (firmware 13.52): AnyPad patches the driver while it runs and puts it back when the option is turned off. Pads are chosen by families (the driver's list holds six ids, one family at a time: `sony` by default, `switch` for the Switch Pro, written in `/data/anypad/bt`). Tested on a PS4 (13.52): the DualSense gives sticks, buttons, touchpad, light bar and player LED; the Switch Pro gives sticks, d-pad, buttons and triggers, with A as the cross. The pad has to connect after the option is on. Rumble from games does not reach either of them yet. Do not use it together with OneFamily, which patches the same words. Xbox pads are out: they use Bluetooth LE and the console's radio is classic.
✅ Un DualSense o un Switch Pro ya enlazados con la consola, con la opción del menú encendida (firmware 13.52): AnyPad parchea el driver mientras está en marcha y lo deja como estaba al apagar la opción. Los mandos van por familias (la lista del driver tiene seis identificadores, una familia a la vez: `sony` por defecto, `switch` para el Switch Pro, escrito en `/data/anypad/bt`). Probado en una PS4 (13.52): el DualSense da sticks, botones, panel táctil, barra de luz y LED de jugador; el Switch Pro da sticks, cruceta, botones y gatillos, con A como cruz. El mando tiene que conectarse después de encender la opción. El rumble de los juegos aún no llega a ninguno. No lo uses a la vez que OneFamily, que parchea las mismas palabras. Los mandos Xbox quedan fuera: usan Bluetooth LE y la radio de la consola es clásica.

| Controller / Mando | Id | Status / Estado |
| --- | --- | --- |
| Sony DualSense, over the console's radio / por la radio de la consola | `054c:0ce6` | ✅ |
| Nintendo Switch Pro, over the console's radio / por la radio de la consola | `057e:2009` | ✅ |

## Reporting a controller / Informar de un mando

Send its vendor and product id (the log prints them when it is plugged in: `usb_pad: usb:X.Y (VVVV:PPPP): ...`), the AnyPad version (first line of the log) and the relevant log lines. See [AI-WORKFLOW.md](AI-WORKFLOW.md).
Envía su id de fabricante y de producto (el log los escribe al conectarlo: `usb_pad: usb:X.Y (VVVV:PPPP): ...`), la versión de AnyPad (primera línea del log) y las líneas del log que importen. Ver [AI-WORKFLOW.md](AI-WORKFLOW.md).
