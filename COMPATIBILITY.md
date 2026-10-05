# Compatibility / Compatibilidad

Every controller below has its own entry in the code, listed one by one.
Cada mando de abajo tiene su propia entrada en el código, uno por uno.

| Symbol | Meaning / Significado |
|---|---|
| ✅ | **Verified** on a real PS5 by a person / **Verificado** en una PS5 real por una persona |
| 🧪 | **Supported, not yet tested**: you can use this controller on the PS5; the support is written and passes the simulator tests, but nobody has tried it on a console yet / **Soportado, aún sin probar**: puedes usar este mando en la PS5; el soporte está escrito y pasa las pruebas con simulador, pero nadie lo ha probado todavía en una consola |

**Today: 1 verified controller, 48 more supported ids, plus the generic profile for any other Bluetooth HID gamepad.**
**Hoy: 1 mando verificado, 48 ids más soportados, más el perfil genérico para cualquier otro mando HID Bluetooth.**

Verified on a PS5 fat, firmware 10.01, with the latest ShadowMountPlus and kstuff-lite.
Verificado en una PS5 fat, firmware 10.01, con el último ShadowMountPlus y kstuff-lite.

## Sony DualShock 4 family / Familia DualShock 4

| USB id | Controller / Mando | Status |
|---|---|---|
| `054c:09cc` | Sony DualShock 4 v2 (CUH-ZCT2) | ✅ Verified / Verificado |
| `054c:05c4` | Sony DualShock 4 v1 (CUH-ZCT1) | 🧪 |
| `0f0d:00f6` | Hori Onyx | 🧪 |
| `1532:1009` | Razer Raiju Ultimate | 🧪 |
| `1532:100a` | Razer Raiju Tournament Edition | 🧪 |
| `2e95:7725` | SCUF Vantage 2 | 🧪 |

## Sony DualSense family / Familia DualSense

| USB id | Controller / Mando | Status |
|---|---|---|
| `054c:0ce6` | Sony DualSense | 🧪 |
| `054c:0df2` | Sony DualSense Edge | 🧪 |

## Xbox

| USB id | Controller / Mando | Status |
|---|---|---|
| `045e:02e0` | Xbox One S (Bluetooth Classic firmware) | 🧪 |
| `045e:02fd` | Xbox One S (Bluetooth Classic firmware, 2nd id) | 🧪 |
| `045e:0b00` | Xbox Elite Series 2 (Bluetooth Classic firmware) | 🧪 |
| `045e:0b05` | Xbox Elite Series 2 (Bluetooth Classic firmware, 2nd id) | 🧪 |
| `045e:0b0a` | Xbox Adaptive Controller (Bluetooth Classic firmware) | 🧪 |
| `045e:0b13` | Xbox Series X|S (Bluetooth LE) | 🧪 |
| `045e:0b20` | Xbox One S, current firmware (Bluetooth LE) | 🧪 |
| `045e:0b21` | Xbox Adaptive Controller (Bluetooth LE) | 🧪 |
| `045e:0b22` | Xbox Elite Series 2 (Bluetooth LE) | 🧪 |

Bluetooth LE models need Secure Connections pairing; whether the PS5's chip supports every step is unverified.
Los modelos Bluetooth LE necesitan emparejado con Secure Connections; si el chip de la PS5 admite todos los pasos no está verificado.

## Nintendo Switch

| USB id | Controller / Mando | Status |
|---|---|---|
| `057e:2009` | Nintendo Switch Pro Controller (and pads that emulate it) | 🧪 |
| `057e:2017` | Nintendo Switch Online SNES controller | 🧪 |
| `057e:2019` | Nintendo Switch Online N64 controller | 🧪 |
| `057e:201a` | Nintendo Switch Online Mega Drive / Genesis controller | 🧪 |

Rumble is not done yet for these. / La vibración aún no está hecha para estos.

## Third-party pads with a built-in mapping / Mandos de terceros con mapa integrado

The layout is read from each pad's own HID descriptor, and corrected where needed.
La disposición se lee del descriptor HID de cada mando y se corrige donde hace falta.

| USB id | Controller / Mando | Status |
|---|---|---|
| `ffff:046e` | GameSir G3s | 🧪 |
| `05ac:022d` | GameSir G3s (alternate mode) | 🧪 |
| `ffff:046f` | GameSir G4s | 🧪 |
| `3537:1022` | GameSir G7 Pro | 🧪 |
| `ffff:0450` | GameSir T1s | 🧪 |
| `05ac:056b` | GameSir T2a | 🧪 |
| `20bc:5501` | Betop 2585N2 | 🧪 |
| `2e2c:0002` | Bionik Vulkan | 🧪 |
| `0079:181c` | LanShen X1Pro | 🧪 |
| `2717:3144` | Xiaomi Mi Controller | 🧪 |
| `1949:0402` | Amazon Fire / iPega controller | 🧪 |
| `2dc8:2100` | 8BitDo SN30 Pro for Xbox Cloud Gaming | 🧪 |
| `2dc8:2101` | 8BitDo SN30 Pro for Xbox Cloud Gaming (2nd id) | 🧪 |
| `2dc8:3012` | 8BitDo Ultimate 2.4G | 🧪 |
| `1949:0403` | iPega 9-series | 🧪 |
| `05ac:022c` | iPega 9-series (alternate id) | 🧪 |
| `1038:1412` | SteelSeries | 🧪 |
| `0111:1420` | SteelSeries (2nd id) | 🧪 |
| `0111:1431` | SteelSeries (3rd id) | 🧪 |
| `0111:1419` | SteelSeries (4th id) | 🧪 |
| `0955:7214` | NVIDIA Shield controller (2017) | 🧪 |
| `1532:0900` | Razer Serval | 🧪 |
| `20d6:89e5` | PowerA MOGA Hero | 🧪 |
| `20d6:0dad` | PowerA MOGA Pro | 🧪 |
| `20d6:6271` | PowerA MOGA Pro 2 | 🧪 |
| `3250:1002` | Atari VCS Modern controller | 🧪 |
| `2e24:200a` | Hyperkin Scout | 🧪 |
| `04e8:046e` | Mocute 050 | 🧪 |

## Any other Bluetooth HID gamepad / Cualquier otro mando HID Bluetooth

The generic profile reads the pad's HID descriptor and applies a default mapping (Android/Xbox or DirectInput order). Adjust it with a `.map` file (see `maps/EXAMPLE.map`).
El perfil genérico lee el descriptor HID y aplica un mapeo por defecto (orden Android/Xbox o DirectInput). Se ajusta con un fichero `.map` (ver `maps/EXAMPLE.map`).

## Not supported yet / Aún no soportado

DualShock 3, Wii Remote / Wii U Pro, Joy-Con used alone.
DualShock 3, Wii Remote / Wii U Pro, Joy-Con por separado.

## Help us verify / Ayúdanos a verificar

Tried one? Open an issue with the model, the console firmware, what worked and `/data/anypad/anypad.log`.
¿Has probado alguno? Abre un issue con el modelo, el firmware de la consola, qué funcionó y `/data/anypad/anypad.log`.
