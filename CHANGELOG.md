# Changelog

Every ELF is `dist/AnyPad-PS4-<version>.elf`. The version is set in
`src/version.h` and is also in the first line of the log, in the start-up
notification, on the web page, and in the binary
(`strings AnyPad-PS4-*.elf | grep anypad-version`).

AnyPad PS4 is a port of [AnyPad PS5](https://codeberg.org/elmonomalva0/Any-Pad-ps5)
by elmonomalvad0. The history of AnyPad PS5 (0.1 to 0.9.5-beta) is in that
project and in this repository's git history.

## 0.1.1-beta (18 October 2026 / 18 octubre 2026)

- When the network changes (the console gets another address, the cable or Wi-Fi comes back), the network controller is served again too, not only the menu, and both are checked every 5 s to really answer: the console can leave a listener dead with AnyPad still running. The notification of a new address gives both addresses.
  Cuando cambia la red (la consola recibe otra dirección, vuelve el cable o el Wi-Fi), también se vuelve a servir el mando por red, no solo el menú, y cada 5 s se comprueba que los dos responden de verdad: la consola puede dejar muerto un servidor con AnyPad en marcha. La notificación de dirección nueva da las dos direcciones.
- The network controller's page gives up on a connection that does not open in 4 s and tries again. After the console's network came back, one controller took a minute to come back: a connection tried while the console had no network hung for that long, and browsers open one connection to an address at a time. Now they come back within seconds.
  La página del mando por red abandona una conexión que no se abre en 4 s y lo vuelve a intentar. Al volver la red de la consola, un mando tardaba un minuto en volver: una conexión intentada mientras la consola no tenía red se quedaba colgada ese tiempo, y los navegadores abren una conexión a la vez hacia una dirección. Ahora vuelven en segundos.
- When none of its controllers has reached the console for 15 s, the network controller's page says so and offers a box for the console's new IP (the one in the notification), which opens the page there. It cannot find the new address by itself.
  Cuando ninguno de sus mandos llega a la consola en 15 s, la página del mando por red lo dice y ofrece una casilla para la IP nueva de la consola (la de la notificación), que abre la página ahí. No puede encontrar la dirección nueva por sí sola.
- README: download from Releases and copy to `/data/payloads`; troubleshooting (same network, the console's IP changing, the controller limit).
  README: descargar de Releases y copiar en `/data/payloads`; problemas frecuentes (misma red, cambio de IP de la consola, límite de mandos).

## 0.1.0-beta (18 October 2026 / 18 octubre 2026) — the PS4 port / el port a PS4

**Controllers / Mandos**

- USB controllers through `libSceUsbd` (the PS4 has no ugen nodes): Xbox 360 (XInput), Xbox One (GIP) and HID, one reader thread per pad. The system driver holding the interface is detached first: that is what made the console see a pad as a headset or a keyboard.
  Mandos USB con `libSceUsbd` (la PS4 no tiene nodos ugen): Xbox 360 (XInput), Xbox One (GIP) y HID, un hilo lector por mando. Antes se suelta el driver del sistema que tenía la interfaz: por eso la consola veía un mando como auriculares o como teclado.
- Each controller becomes a virtual DualShock 4 (device type 1), created with its first report. It is not bound to a user (`sceMbusBindDeviceWithUserId` faults on the PS4): the console asks who is using it.
  Cada mando es un DualShock 4 virtual (tipo 1), creado con su primer informe. No se asigna a un usuario (`sceMbusBindDeviceWithUserId` falla en PS4): la consola pregunta quién lo usa.
- GameSir profile (report `0x05`) for the Kaleid Flux; Switch Pro over USB with its handshake and a keep-alive every 500 ms (the GameSir G8 Plus in Switch mode dropped every 10–25 s without it).
  Perfil GameSir (informe `0x05`) para el Kaleid Flux; Switch Pro por USB con su handshake y un keep-alive cada 500 ms (el GameSir G8 Plus en modo Switch se caía cada 10–25 s sin él).
- Virtual pads are asked for 2.5 s apart, and their kernel log line is waited for up to 10 s. Asked a few ms after the previous one, a device was made but its line never came; asked again, the first stayed on the console as a pad nobody drives, and the console waited on it to say who is using it ("cannot connect any more controllers").
  Los mandos virtuales se piden con 2,5 s de separación, y su línea del log del kernel se espera hasta 10 s. Pedido a pocos ms del anterior, el dispositivo se creaba pero su línea no llegaba; al pedirlo otra vez, el primero se quedaba en la consola como un mando que nadie maneja, y la consola esperaba a que dijera quién lo usa («no se pueden conectar más mandos»).
- Sony's own controllers are left to the console, which keeps them for itself.
  Los mandos de Sony se dejan a la consola, que se los reserva.

**Network controller / Mando por red**

- A phone or PC on the local network opens `http://<ip>:8096/`; its browser reads a controller (plugged in, or paired to the phone by Bluetooth) and sends it over a WebSocket. Each client drives a virtual pad. Local network only. Several controllers on one phone: the page opens one connection per controller, so each is a virtual pad of its own, shown in the menu by the name its browser gives it. The page keeps the phone's screen awake. The idea comes from [Control4Free](https://github.com/MoHadiShibli/Control4Free).
  Un móvil o PC en la red local abre `http://<ip>:8096/`; su navegador lee un mando (conectado, o emparejado con el móvil por Bluetooth) y lo envía por WebSocket. Cada cliente maneja un mando virtual. Solo red local. Varios mandos en un mismo móvil: la página abre una conexión por mando, así que cada uno es un mando virtual distinto, que el menú muestra con el nombre que le da su navegador. La página mantiene encendida la pantalla del móvil. La idea viene de Control4Free.

**PS4 only / Solo PS4**

- Removed what only served the PS5: the Bluetooth stack (pairing, reconnection, L2CAP, SDP, HID, LE and its cryptography, the HCI transport over the shared chip), the PS5 build and its ugen USB path, the console hotkey and `config.ini`, and «Configure PS». Files and names are `ps4_*`.
  Se quitó lo que solo servía en PS5: la pila Bluetooth (emparejado, reconexión, L2CAP, SDP, HID, LE y su criptografía, el transporte HCI por el chip compartido), el build de PS5 y su camino USB por ugen, el atajo de consola y `config.ini`, y «Configurar PS». Los ficheros y nombres son `ps4_*`.
- The menu shows the USB and network controllers live, the network controller's address and **Press PS** per controller; the pairing pages are gone.
  El menú muestra en vivo los mandos USB y de red, la dirección del mando por red y **Pulsar PS** por mando; las páginas de emparejado ya no están.
- The link stubs for `libSceUsbd` and `libSceMbus` are part of the repository (`tools/sce_stubs/`) and the build makes them; `make ps4` and the Docker script share `ps4.mk`.
  Los stubs de enlace de `libSceUsbd` y `libSceMbus` forman parte del repositorio (`tools/sce_stubs/`) y el build los compila; `make ps4` y el script de Docker comparten `ps4.mk`.
