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
| Sony DualShock 4, DualSense | `054c:…` | left to the console: it keeps them for itself and they work natively / se los deja a la consola: se los reserva y ya funcionan de forma nativa | ❌ |

The GameSir G8 Plus picks its USB mode by itself when plugged in; both modes work. Its own PS4 mode is a native console controller and does not go through AnyPad.
El GameSir G8 Plus elige solo su modo USB al conectarlo; los dos modos funcionan. Su modo PS4 propio es un mando nativo de la consola y no pasa por AnyPad.

A controller's headphone jack is handled by the console, not by AnyPad.
El conector de auriculares de un mando lo gestiona la consola, no AnyPad.

## Over the network / Por la red

The page at `http://<ip>:8096/` sends whatever controller the phone's or PC's browser reads (Gamepad API, standard mapping): plugged into that device, or paired to it by Bluetooth.
La página `http://<ip>:8096/` envía el mando que lea el navegador del móvil o del PC (Gamepad API, mapeo estándar): conectado a ese dispositivo o emparejado con él por Bluetooth.

| | Status / Estado |
| --- | --- |
| Virtual pad created and fed from a test client / Pad virtual creado y alimentado desde un cliente de prueba | ✅ |
| A real phone with a real controller / Un móvil real con un mando real | ⚠️ |

## Over the console's Bluetooth / Por el Bluetooth de la consola

❌ The system owns the console's Bluetooth radio and its controller connections; a payload cannot read them. For wireless play, pair the controller to a phone and use the network controller.
❌ El sistema es el dueño de la radio Bluetooth de la consola y de sus conexiones de mandos; un payload no puede leerlas. Para jugar sin cable, empareja el mando con un móvil y usa el mando por red.

## Reporting a controller / Informar de un mando

Send its vendor and product id (the log prints them when it is plugged in: `usb_pad: usb:X.Y (VVVV:PPPP): ...`), the AnyPad version (first line of the log) and the relevant log lines. See [AI-WORKFLOW.md](AI-WORKFLOW.md).
Envía su id de fabricante y de producto (el log los escribe al conectarlo: `usb_pad: usb:X.Y (VVVV:PPPP): ...`), la versión de AnyPad (primera línea del log) y las líneas del log que importen. Ver [AI-WORKFLOW.md](AI-WORKFLOW.md).
