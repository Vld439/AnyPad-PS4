# Plan: mandos Bluetooth en AnyPad (fw 13.52), vía allowlist de `/dev/bt`

Sustituye al plan de 2026-10 (`BT-PLAN-2026-10.md`, que concluyó "userland cerrado, hace falta
host L2CAP/HIDP propio"). Esa conclusión era errónea: faltaba un parche de **datos** en el
allowlist del driver de `/dev/bt` y el registro del VID/PID. Lo demuestra OneFamily v1.2.1
(github.com/ChiricoG/OneFamily, MIT, `daemon/src/bt_driver.c`; base: Diramix/DualSense-PS4).

Objetivo: (1) DualSense/Edge por BT → DS4 virtual; (2) después, probar Pro Controller.

---

## Mecanismo (de OneFamily, comprobado contra nuestro dump 13.52)

1. El driver de `/dev/bt` solo abre el canal HID de un mando cuyo VID/PID esté en un allowlist
   cableado en el código: 6 funciones comparan `vid==0x044F && pid==0x1000` (Thrustmaster) o
   `vid==0x06F8 && pid∈{0xE051,0xE052}` (Guillemot, `add pid,0x1faf; cmp 2; jb`). VID en
   `+0x28`, PID en `+0x2a`, y `byte +0x2f == 2`.
2. Parche: cambiar los inmediatos `0x044F→0x054C` y `0x1000→0x0CE6` (12 dwords). Offsets 13.52
   (`base` = `rdmsr(0xC0000082) - 0x1C0`), **verificados** en `~/ps4-bt-research/kernel.bin`:
   VID `0x59C869 0x5B1616 0x5B1E86 0x5B5C48 0x5B5FFD 0x5B607B`,
   PID `0x59C8C0 0x5B1646 0x5B1EB9 0x5B64DE 0x5B656E 0x5B348A`.
   Escritura de `.text` en kexec con `cli` + CR0.WP=0; comprobar antes que cada dword tiene el
   valor original o el parcheado (si hay mezcla, abortar: otro payload lo tocó a medias).
   En 13.52 OneFamily no toma lock (en 12.50 sí: `0x5A6B80/0x5A6C00`).
3. Usuario, en este orden: `open("/dev/bt", O_RDWR)` → kqueue `EVFILT_READ` → `write` de la
   máscara `0x1FF000000` (8 B) → parche → `ioctl(0x8004622B, {u16 vid, u16 pid})` (registro;
   `0x8004622C` lo quita).
4. Lectura: `read()` da eventos de 16 B; tipos `0x18..0x20` traen en `[8..15]` la **dirección**
   del dispositivo (no es MAC ni handle: por eso nuestro `0x80206233` daba `0x802F3703`).
   `ioctl(0x80206233, {u64 addr, void *buf, u64 cap=80, u64 0})` devuelve `n`;
   válido si `buf[0]+2 == n && buf[1] == 0x1F`; el informe HID empieza en `buf+2`
   (`0x31` de 78 B con CRC, o `0x01` de 10 B) → **tal cual** a nuestro `ds5_parse` (`r[0]` = id).
5. Salida (rumble/LED): `ioctl(0x80206232, {addr, buf, 78, 0x31})` con el `0x31` de 78 B sellado
   (CRC prefijo `0xA2`). El informe BT no es el de USB: se arma en `bt_proto.c` con los offsets de
   OneFamily.

Limitaciones conocidas (issues de OneFamily): #29 un DualSense por cada `0x8004622B` (para 2+
mandos, un registro por mando); #39 cortes en 12.00; #25 reconocido sin entrada (sin datos).

---

## Estado (2026-11-08)

- **F1 hecho y probado en consola (fw 13.52, 2026-11-08)**: `src/bt_proto.c` (envoltorio del driver,
  CRC HID, informe de salida) y `src/bt_pad.c` (parche, registro, sondeo, slots). Menú: casilla
  **Bluetooth de la consola** (`/data/anypad/bt`, `POST /api/bt?on=1`). `make test`, `make fuzz` y
  `make ps4` pasan. En la PS4: el DualSense se engancha, sticks/botones/panel llegan al DualShock 4
  virtual, la consola navega con él, la barra de luz y el LED de jugador se encienden (informe `0x31`
  de salida de 78 B) y el enlace no se cae. Parche y restauración comprobados con una sonda aparte
  (`kexec`, lectura de los 12 dwords): `044F/1000` ⇄ `054C/0CE6`, y tras apagar la opción vuelven a
  `044F/1000`. Dos fallos encontrados y corregidos en la prueba: la comprobación de firma impedía
  restaurar (`0xE3`, la propia firma ya estaba parcheada) y el cierre daba "restored" sin mirar el
  código de retorno del kernel.
- Pendiente de esa prueba: el rumble de los juegos (AnyPad no recibe salida todavía) y el Edge
  (`054C:0DF2` se rechaza con ENOENT, sin investigar).
## Otros mandos (2026-11-08)

**Funciona el Switch Pro por Bluetooth de la consola** (probado en 13.52):

- El fallo que lo bloqueaba era nuestro: `bt_kernel_patch()` escribía siempre `054C:0CE6` en el
  allowlist, aunque la familia pidiera otro id; el registro ya usaba los ids de la familia. Ahora el
  parche lleva `req.vid/req.pid` y reconoce como "ya parcheado" tanto el par de Sony (nuestro o de
  OneFamily) como el pedido.
- `ENOENT` al registrar **no** es un fallo del driver: significa que no hay ningún mando conectado
  con ese id (por eso el Edge daba ENOENT: no había un Edge). Con el Switch Pro enlazado y el
  allowlist a `057E:2009`, `register` responde 0.
- El driver **sí entrega informes de mandos ajenos**: el Switch Pro manda su informe HID estándar
  `0x3F` de 49 bytes (`3F 00 00 08 00 80 00 80 …`, sticks a 0x8000, hat 8 = neutro).
- La familia `switch` reutiliza el perfil USB de `profiles.c` (mismo parser de `0x3F`), así que se
  engancha como `Switch Pro (Bluetooth)` y los sticks llegan centrados.
- El mando tiene que **reconectarse después** de que arranque el módulo: si ya estaba conectado, el
  driver no anuncia su dirección al nuevo suscriptor y no llega nada (con el DualSense pasa igual).
- **Funciona en consola (2026-11-08)**: se juega y se navega con el Switch Pro. Dos ajustes que lo
  hicieron falta, además del parche: (1) el pad virtual se alimenta **cada 16 ms** aunque el mando no
  mande informes (el Switch Pro solo manda al cambiar algo, y con el flujo a saltos la consola no
  seguía el estado); (2) los botones se remapean al estilo PlayStation (A = cruz/confirmar, B =
  círculo, X/Y en su sitio), porque en el mando de Nintendo la A está a la derecha.
- **Xbox: no es viable por ahora.** Los mandos en modo Xbox se presentan como Xbox Series, que es
  **Bluetooth LE**, y la radio de la PS4 (y su stack) es Bluetooth clásico: no se pueden emparejar
  (no salen ni en el escáner). La familia `xbox` pide los ids clásicos y de Series; en la prueba
  `045E:02FD` registró porque quedaba un enlace viejo en la consola, pero no llegó ningún informe.
  Para mandos ajenos, el camino es un modo que use Bluetooth clásico HID (Switch, o el modo PS4 si
  el mando lo tiene, que la consola acepta directamente).
- Falta: rumble y luz para la familia `switch` (no se manda ningún informe de salida).

## Otros mandos, notas del plan original (2026-11-08, sustituido)

Lo que se sabe y lo que falta:

- El allowlist tiene sitio para **6 pares VID/PID**; el parche escribe los 12 dwords, así que se
  pueden habilitar como mucho seis identificadores a la vez (AnyPad tiene ~40 perfiles). No caben
  todos: se elige una **familia**.
- El driver entrega los informes **por dirección**, nunca por VID/PID: no se puede saber de qué
  mando es un informe. Por eso el módulo trabaja con una sola familia a la vez (`bt_family` en
  `bt_proto.c`: ids, `accept()` y `parse()`). El fichero `/data/anypad/bt` puede llevar el nombre
  de la familia (`sony` por defecto; vacío = `sony`), que es lo que lee `ps4_main.c`.
- Implementado ya: `bt_family`, registro de los ids de la familia, y un volcado en el log de los
  primeros informes que una familia no reconoce (`bt_pad: <addr>: not <familia> (n bytes): HEX`),
  para poder escribir el parser de otro mando con datos reales.
- **Familias de descubrimiento** (añadidas 2026-11-08): `watch_accept()` no acepta ningún informe, así
  que `bt_pad.c` escribe en el log lo que llegue (`bt_pad: <addr>: not <familia> (n bytes): HEX`) y no
  se crea ningún pad. `xbox` pide solo `045E:0B13` y `switch` solo `057E:2009`: un id por familia, un
  solo cambio a la vez. Todo el camino (parche validado dword a dword, registro, lectura) es el mismo
  código ya probado con el DualSense; revertir es escribir `sony` en `/data/anypad/bt`.
  **Sin probar en consola.** No usar la sonda `bt2.bin`: escribe el kernel a ciegas y congeló la
  consola (2026-11-08).
- **Sin comprobar todavía** (bloqueado por el cargador de payloads): si el driver entrega informes
  de un mando que no es Sony, y sobre todo si la consola **empareja** un mando ajeno (el parche
  solo deja que el driver acepte un enlace ya emparejado; el menú de Bluetooth de la PS4 no ofrece
  mandos que no sean de Sony). Sonda lista: `/data/payloads/bt2.bin`
  (fuente `/tmp/opencode/btprobe2/probe.c`, tabla `g_ids` para cambiar los ids), que parchea,
  registra y vuelca eventos + informes en `/data/anypad/anypad.log`.
- Si el enlace no llega a formarse, hace falta emparejar por otra vía (tocar el stack de Bluetooth
  de la consola), que es un trabajo aparte y bastante mayor.

- **Una sola instancia**: con otro AnyPad autoarrancado (`/data/GoldHEN/payloads.ini`) el segundo
  proceso se queda con `/dev/bt` y el mando no entra. El mando, además, tiene que encenderse
  **después** del módulo: si quedó enlazado del arranque anterior no reconecta solo.
- Falta en el plan original: el **parche de capacidad por enlace** (`ctx+0x42A8` + n·`0x1588`,
  MAC en `+0x30`, capacidad en `+0x1574`: 22→78), necesario en 13.52 para que el DualSense no corte
  el enlace al recibir un `0x31` de 78 B. Se aplica por dirección al primer informe y se restaura
  al cerrar.
- Salida: los juegos no dan salida a los mandos de AnyPad todavía, así que solo sale el `0x31`
  neutro (keep-alive: el primero a los 20 ms, luego cada 2,5 s). `bt_pad_output()` es el enganche.
- F0 (sonda aparte) ya no hace falta: offsets verificados contra el dump 13.52 y la mecánica es la
  de OneFamily `dev` v1.2.2-beta11.

---

## Fases

### F0. Sonda `btds5` (payload aparte en `~/ps4-bt-research/`, sin tocar AnyPad)
- Pasos 1-5 con log a `/data/btprobe/ds5.log`: valores leídos antes/después del parche,
  retorno del registro, eventos (tipo + dirección), 20 informes crudos, CRC.
- Restaurar el parche y desregistrar al terminar; **nunca `exit()`** (volver de `main`).
- Comprobar que un DS4 real sigue funcionando con el parche puesto.
- Gate: llegan informes `0x31` con CRC correcto al pulsar el DualSense ya emparejado.

### F1. Fuente BT en AnyPad
- `src/bt_pad.c/.h` (solo `__ORBIS__`), mismo contrato que `usb_pad`: `bt_pad_init(feed, attach,
  detach)`, `bt_pad_poll(now)` sin bloquear (kevent con timeout 0), `bt_pad_output(...)`.
- Kernel: `bt_patch(restore)` con la tabla 13.52 y verificación de los 12 valores; otros
  firmwares → no se activa (sin escáner AOB por ahora).
- Por dirección: `attach` al primer informe válido (perfil por VID/PID registrado), `detach` a 3 s
  sin informes; salida por `0x80206232`.
- `src/ps4_main.c`: arrancar junto a `usb_pad_init`; al parar AnyPad, desregistrar y restaurar.
- Avisos por `TR()` (EN/ES). Tests de host: parser del envoltorio (`buf[0]+2`, `0x1F`, CRC) en
  `tests/`, con informes capturados en F0.
- Gate: el DualSense mueve un juego vía DS4 virtual; rumble y barra de luz.

### F2. Varios DualSense y convivencia
- Registro por mando (issue #29); dos DualSense a la vez.
- Si OneFamily (u otro) ya dejó el parche puesto: aceptarlo y no restaurarlo al salir.

### F3. Pro Controller (experimento)
- Usar la rama Guillemot (`0x06F8`, rango por `add`) para un segundo VID/PID sin quitar el
  DualSense: `0x06F8→0x057E` y el `add 0x1faf` → `-0x2009` (PID 0x2009/0x200A). Primero ver en el
  dump cuántas de las 6 funciones tienen esa rama.
- Registro `0x8004622B {0x057E,0x2009}`; el Pro empieza en `0x3F` y necesita subcomandos por la
  salida para pasar a `0x30`: `switch_parse`/`switch_init` ya existen en `src/profiles.c`.
- Gate: llega algún informe del Pro. Si no, Pro por BT queda fuera.

### F4. Otros firmwares (opcional)
- Tablas por firmware o escáner por patrón (VID `0x044F` con `cmp` en `[0x4E0000,0x660000)`),
  solo si hay testers con ese firmware.

---

## Riesgos
- Escribir `.text` del kernel: verificar los 12 valores antes; ventana con `cli` mínima.
- Un payload anterior que parcheó y murió deja el kernel parcheado: tratarlo como estado válido.
- Hilo de AnyPad que se cuelga en el `ioctl`: todo no bloqueante; si no, hilo aparte.
- Consola con 5 cuelgues previos en el sondeo BT: probar F0 con la consola recién reiniciada.
