# Changelog

Every ELF is `dist/AnyPad-PS5-<version>.elf`, with a `.sha256` beside it. The
version is set in `src/version.h` and is also in the first line of the log, in
the start-up notification, on the web page, and in the binary
(`strings AnyPad-PS5-*.elf | grep anypad-version`).

## 0.9.5-beta (14 October 2026 / 14 octubre 2026)

The same program as 0.9.1-beta; only the version number changes. What is new is that **the whole source code is public** (GPL-3.0-or-later), with `AI-WORKFLOW.md` (how to work on AnyPad with an AI assistant: connect to the running program, watch the log in real time, change live settings, add a controller, test it and report it), `tools/live.sh` (the commands to talk to AnyPad over the local network) and an issue template for controller reports. Not in the source tree: the internal notes, the old ELFs and the icon tools.

Es el mismo programa que la 0.9.1-beta; solo cambia el número de versión. Lo nuevo es que **todo el código fuente es público** (GPL-3.0-or-later), con `AI-WORKFLOW.md` (cómo trabajar en AnyPad con un asistente de IA: conectarse al programa en marcha, ver el log en tiempo real, cambiar ajustes en caliente, añadir un mando, probarlo e informar), `tools/live.sh` (las órdenes para hablar con AnyPad por la red local) y una plantilla de incidencias para informes de mandos.

## 0.9.1-beta (12 octubre 2026)

- **El mando virtual se crea con el primer informe del mando**, no al quedar listo su enlace. Un mando que se enlaza pero no envía nada (el 8BitDo al encenderlo de nuevo con AnyPad en marcha) ya no deja un DualSense virtual quieto en el que la consola se queda esperando sin aceptar los demás mandos. Prueba `test_silent_pad_no_virtual` (falla con el comportamiento anterior). Las órdenes que AnyPad envía a los mandos son las mismas que en la 0.8.1.
- Log: `first input (report …): virtual DualSense created`, y en «not in its full report yet» los informes que el mando ha enviado (para entender por qué no se despierta).

## 0.9.0-beta (12 octubre 2026)

Versión de compatibilidad. El camino de DS4 y 8BitDo en la consola MediaTek es el de la 0.8.1 (misma huella de órdenes en el simulador); todo lo nuevo está aislado por modelo, por chip o por caso, y probado con mandos emulados (`tests/test_compat.c`, cada arreglo comprobado con mutación). Sin probar en consola con esos mandos: beta.

- Todo lo de la 0.8.2, 0.8.3 y 0.8.4 (Xbox por LE: identidad y Exit Suspend; LE con el chip ocupado; clones de DS4 con calibración; chips Marvell/NXP; icono por NID en firmwares nuevos; interfaz nueva).
- Interfaz en naranja (el color de AnyPad); la pausa por el DualSense en naranja con su motivo; guía de emparejado con Joy-Con, Wii U Pro y Wiimote, y Stadia.
- Clones que no dan su identificador: reconocidos por el nombre que anuncian («Wireless Controller», «Pro Controller», «Xbox Wireless Controller»).
- Joy-Con sueltos (L y R, y los NES Online) de lado; segundo id del mando Mega Drive Online.
- Reconexión: un mando que llama mientras su enlace sigue abierto aquí (cayó sin aviso) suelta el enlace muerto y se acepta la llamada (el 8BitDo que se dormía y no volvía).
- Wii U Pro y Wiimote: PIN automático con la dirección de la consola (método Sync), «0000» para los demás y la dirección si un mando rechaza «0000»; extensión, modo 0x34; el perfil repite su arranque tras un aviso de estado.

## 0.8.4-beta (11 octubre 2026)

Versión de prueba con todo lo de compatibilidad hecho hasta ahora y la interfaz nueva. El camino de DS4 y 8BitDo en la consola MediaTek no cambia (misma huella de órdenes que la 0.8.1).

- Clones de DS4 que solo pasan al informe completo al pedirles la calibración (feature 0x05), en los reintentos. HORI Wireless Controller Light.
- Consolas con chip Marvell/NXP (1286:2059): busca el chip en varios nodos, salida de datos según el chip, no toca la radio del sistema en Marvell, aviso de arranque en frío. Sin probar en una Marvell.
- Icono en la fila de multimedia en firmwares nuevos: el instalador se busca por NID y, si no, se usa la instalación general (como ps5-payload-manager).
- Interfaz nueva: menú lateral, radar Bluetooth con barra de progreso, azul con «Espera: DualSense» durante la pausa, tarjetas de mando con batería, guía de emparejado por mando en 12 idiomas, chip visible.

## 0.8.3-beta (10 octubre 2026)

Segunda de compatibilidad: conexión de mandos **Bluetooth LE** cuando el chip está ocupado. El camino Classic (DS4, 8BitDo, Switch Pro) no cambia: misma huella de órdenes que la 0.8.1.

- Si el chip responde **0x0c** («hay otro intento de conexión LE en curso»: del sistema o uno que dejó una ejecución anterior) a nuestra conexión LE, ya no se suelta el mando para repetir el mismo intento en el siguiente anuncio (en el simulador: 266 intentos en 8 s sin conectar nunca, el fallo del issue nº 1 con el Xbox Series). Ahora se cancela el intento pendiente (`LE Create Connection Cancel`) y se vuelve a pedir a los 300, 800 y 1500 ms; tras el cuarto rechazo, ese mando descansa 5 s.
- Solo se atienden las respuestas a nuestra propia orden de conexión LE; las del sistema se apuntan y se ignoran. El fin del intento cancelado (Connection Complete 0x02) no tumba nuestro enlace.
- Al parar AnyPad se cancela un intento LE nuestro que siguiera pendiente, para que no bloquee la siguiente ejecución.
- Log: `the controller was busy with another connection attempt: cancelled it, trying again in N ms`, `still refused; left alone for 5 s`.
- Simulador: opción `busy` (intento ajeno pendiente que hay que cancelar N veces); prueba `test_busy_controller` (1, 3 y 5 cancelaciones: 2, 4 y 7 intentos). Falla con el código anterior.
- Idea de PadBridge-PS5 0.1.5 (X-F1REBALL-X, GPL-3.0); código propio. Sin probar con un Xbox real.

## 0.8.2-beta (10 octubre 2026)

Primera versión de compatibilidad sobre la 0.8.1 (base intocable). Solo cambia el emparejado de los mandos **Bluetooth LE**; el camino de los mandos Classic (DS4, 8BitDo, Switch Pro) es el mismo, con la misma huella de órdenes en el simulador.

- **Identidad de la consola al emparejar:** la petición de emparejado ofrece ahora nuestra identidad (`preq[5] = 0x02`). Si el mando la acepta, tras recibir sus claves se le envían la clave de identidad (IRK, creada una vez y guardada en `<pads.db>.irk`) y la dirección pública de la consola. El Xbox Series X|S (`045e:0b13`) sin esto daba el enlace por incompleto: titilaba y no mandaba botones. A los mandos que no la piden no se les envía nada.
- **HID Control Point:** al terminar de activar las notificaciones, a un mando HID por LE que ofrece Control Point se le escribe «Exit Suspend» (una escritura sin respuesta). Los que no lo tienen no cambian.
- Log: `our identity handed over` y `told to leave suspend`.
- Simulador: opciones de mando `wants_identity` y `needs_wake` (Xbox real); pruebas nuevas `test_xbox_identity` (SC y legacy, y vuelta tras apagarlo) y `test_identity_only_if_asked`. Comprobado con mutación: sin cada arreglo, la prueba falla.
- Idea tomada de una compilación de un tercero (AnyPad 0.5.7 con módulos de PadBridge-PS5 de X-F1REBALL-X, GPL-3.0); código reescrito. Sin probar con un Xbox real en esta versión.

## 0.8.1-beta (9 octubre 2026)

La última versión de AnyPad PS5. Es la 0.8.0 con su último retoque: si en un enlace recién hecho el mando niega el canal inicial (SDP) por falta de recursos 3 veces, se hace un enlace nuevo a los 3 s, una vez por búsqueda (el 8BitDo lo hacía en su primer enlace; el segundo funcionaba al momento). Configurar PS (1 botón o 3) probado en consola: funciona.

## 0.8.0-beta (9 octubre 2026)

La 0.6.19 (probada: DS4 y 8BitDo a la vez, «va perfecto», sin lag) pulida, con dos novedades en la página. Nada de la búsqueda ni del emparejado cambia, salvo el pulido del 8BitDo.

- **Configurar PS**: en «Mandos emparejados», a la izquierda de «Pulsar PS», para los mandos que no son DualShock 4 ni DualSense. El usuario elige si hará de PS un botón o una combinación de tres, y los pulsa en el mando: manteniéndolos 1 s se detectan solos (sin desplegables, a petición del usuario). Mientras se mantiene la combinación entera, la consola recibe PS en lugar de esos botones. Se guarda por modelo de mando en `/data/anypad/ps_combos.txt` y sirve en las siguientes sesiones. Mientras el diálogo está abierto, la combinación anterior no actúa.
- **Idiomas**: la página sigue el idioma de la consola: español, inglés, francés, italiano, portugués, alemán, ruso, chino simplificado y tradicional, japonés, coreano y árabe (de derecha a izquierda); inglés para cualquier otro. Comprobada con capturas en todos ellos, en pantalla de tele y de móvil. Los avisos de la consola siguen en español o inglés.
- Pulido del 8BitDo: tras rechazar su propia llamada, se le llama a los 2 s (no 0,7). Si en un enlace recién hecho niega el canal inicial (SDP) por falta de recursos 3 veces, se hace un enlace nuevo a los 3 s, una vez por búsqueda (antes, 8 negativas: unos 20 s). Visto en consola dos veces, con y sin llamada propia; el segundo enlace funcionó siempre al momento.
- Texto de ayuda de la página al día: sin la recomendación del cable USB, con el aviso de no apagar el DualSense.
- «Pulsar PS» tampoco aparece en los DualShock 4 ni en los DualSense (tienen su propio botón PS). Retoque pedido al probarla; build `v0.8.0-beta-1-…`.

## 0.6.19-beta (9 octubre 2026)

Lo visto en directo con la 0.6.18 (DS4 y 8BitDo a la vez):

- Sin lag: la radio se queda ligera mientras haya mandos de AnyPad conectados, no solo al emparejar. Con el page scan pesado del sistema, el DS4 iba «con muuucho lag»; aligerado en vivo, «ahora va bien». Se vuelve a mirar cada 30 s; el valor del sistema se devuelve cuando no queda ningún mando o al parar.
- El 8BitDo que se cree enlazado se empareja a la primera: al encenderlo llama a la consola, su llamada queda pendiente 29 s y la de AnyPad recibe «ya existe» (0x0b). Ahora AnyPad rechaza esa llamada pendiente y lo llama a los 700 ms (hasta 3 veces por búsqueda). Si no había tal llamada (0x02), es un enlace real y se deja como antes. Solo con la dirección que AnyPad quiere emparejar.
- La traza no escribe claves de emparejado (Link Key Notification y Link Key Reply).
- Ajuste en caliente `ev_len`: bytes pedidos en cada lectura de eventos (16 = un paquete, como siempre). Para probar si el lector del sistema deja de atascarse cuando cada lectura recibe eventos enteros.
- DualSense: limitación conocida (decisión del usuario): no apagarlo con AnyPad en marcha.

## 0.6.18-beta (9 octubre 2026)

0.6.17 con lo aprovechable de la auditoría externa y lo visto en la traza del DualSense.

- La máscara sin Connection Request queda desactivada por defecto (experimento del modo diálogo, `mask_conn=1`): en consola el DualSense apagado y encendido se quedó en azul igual.
- Traza del 9 oct: el DualSense vive en la misma función del chip que lee AnyPad (enlace 0x032). Al apagarlo, el aviso de desconexión (`05 04 00 32 00 13`) lo leyó AnyPad, no el sistema: el sistema siguió creyéndolo conectado y no lo dejó volver, ni con AnyPad parado.
- Nuevo ajuste en caliente `idle_reads`: lecturas de eventos que AnyPad mantiene cuando no empareja (40 = como siempre). Con menos, los eventos que nadie de AnyPad espera los recibe el sistema. Al emparejar y al enviar cualquier orden vuelven todas. Los datos de los mandos (ACL) no cambian. Por defecto 40, para probarlo en directo.
- La devolución del page scan se reintenta (cada 3 s, hasta 5 veces) en vez de darse por hecha antes de confirmarla.
- El vigilante de la búsqueda sigue a `inq_len` (no 14 s fijos); `ps_iv` se valida contra `ps_win`; `mask_conn` solo da «ok» si el chip lo confirma.

## 0.6.17-beta (9 octubre 2026)

0.6.16 con un arreglo: una orden del modo diálogo (/api/hci) que espera al chip hacía que la web se volviera a servir desde dentro de sí misma (el host atiende la web mientras espera), la misma petición se repetía sin fin y AnyPad se caía. La web ya no se vuelve a entrar. El test de la web usa ahora la misma espera que la consola; sin el arreglo, se cae igual.

En consola, la 0.6.16 (antes de la caída): con la radio en el modo pesado del sistema, page scan aligerado al pulsar Emparejar, DS4 encontrado a los 9,5 s, contesta a la llamada en 0,47 s, listo a los 13,5 s; la tele vuelve a oírse.

## 0.6.16-beta (9 octubre 2026)

Auditoría de todos los logs (46 ejecuciones, de la 0.5.25 a la 0.6.15): lo que separa una ejecución buena de una mala no es la versión, es cómo deja el sistema la radio. Con page scan 334 de cada 668 ranuras, entrelazado (la radio escucha llamadas casi el 100 % del tiempo), la búsqueda oye 0,34 aparatos por ronda y 16 llamadas acabaron sin respuesta; con el otro valor del sistema (18 de cada 2160), 0,98 por ronda y ninguna llamada fallida. La 0.6.15 del 9 de octubre arrancó en el modo pesado: ni la tele en 60 s y tres llamadas al DS4 sin respuesta.

- Mientras se empareja, el page scan del sistema se aligera a 18 ranuras cada 1,28 s (lo que permite R1: el DualSense sigue reconectando al momento) y se devuelve el valor del sistema al acabar. Se vuelve a mirar cada 3 s, por si el sistema lo cambia.
- Si una ronda entera de búsqueda no oye nada (ni la tele), se registra «radio ocupada» y se vuelve a mirar el page scan en el acto.
- Las llamadas a la consola (Connection Request) quedan para el sistema: AnyPad ya no las recoge (máscara de eventos sin ese bit; se repone al cerrar). Cada mando de la sesión lo llama AnyPad, así que no las necesita. El DualSense apagado y encendido llamaba, AnyPad se quedaba la llamada y nadie la contestaba: azul. Sin verificar en consola.
- Modo diálogo en la web (puerto 8095): /api/live?from=N (log en vivo), /api/tune (ajustes en caliente: ps_light, ps_iv, ps_win, mask_conn, inq_len, trace) y /api/hci?op=XXXX&p=hex (una orden HCI; nunca Reset). Para ajustar en directo, sin una versión por prueba.

## 0.6.15-beta (9 octubre 2026)

Candidata 0.6.14 (configuracion L2CAP adelantada y backoff de rechazos) mas tres cambios pensados para que el emparejado no dependa de reiniciar el Bluetooth ni el mando. Parte de la referencia 0.6.10 (1e784b2).

- Si la llamada al mando acaba sin respuesta (page timeout, estado 0x04), AnyPad vuelve a llamar al mismo mando hasta 2 veces mas, tras 500 ms, ampliando la ventana de emparejado. Antes la llamada fallida terminaba el intento y no se volvia a probar mientras no se pulsara «Emparejar» otra vez.
- Si un paso del emparejado seguro (SSP) no llega en 3 s (peticion de capacidades, confirmacion del usuario), AnyPad contesta el siguiente paso igualmente en vez de esperar un evento que pudo leer el sistema.
- Si al abrir el USB faltan lecturas de eventos o de datos (otro payload sigue reteniendo transferencias del chip, como Prospero-Pad), AnyPad vuelve a pedirlas cada 5 s y se recupera solo cuando el otro suelta. Avisa en pantalla de que otro programa usa el Bluetooth, y tambien si el puerto 8095 esta ocupado.
- Nuevos tests del simulador: repagina tras timeout (3 llamadas, una sola conexion) y perdida de eventos de emparejado (0x31 y 0x33). Mutaciones comprobadas: sin repaginar y sin el empujon SSP, los tests fallan.

Pendiente de prueba en consola. No arregla el DualSense azul ni la sordera del chip.

## 0.6.14-beta-candidate (9 octubre 2026)

Sobre la candidata 0.6.13. Revisión directa de HearBridge-PS5, commit 308995806bfa39861501848b4aee3ecd64b79287 (X-F1REBALL-X, GPL-3.0). Implementación propia de las ideas aplicables; no se copia su capa Bluetooth.

- Conserva una petición de configuración L2CAP completa, sin opciones, recibida antes de conocer el CID remoto. La contesta al recibir la apertura, sin esperar a que el mando la repita. Borra la pendiente al cerrar o abrir un intento nuevo. Inspirado en el tratamiento de CFG_REQ adelantado de HearBridge.
- Tras un rechazo L2CAP, espera 250, 500, 1000, 2000 y hasta 4000 ms entre intentos, con máximo de 8 rechazos por canal; cierra el enlace propio al agotarlos. Es el diseño de backoff de AnyPad 0.6.12, separado de la reserva ACL. El límite de dos intentos de HearBridge se refiere a CREATE_CONNECTION, no a SDP.
- No incorpora reserva ACL, cambio del timeout global, reset del chip, cambio de escaneo ni sustitución del transporte.
- Diez escenarios dirigidos (DS4 y Switch Pro) prueban intercambio normal, configuración adelantada con/sin repetición y rechazo transitorio/persistente. Se han probado cada cambio y la combinación por separado.

Experimental: mejora demostrada en la negociación simulada, no en la primera detección por radio. Pendiente de probar físicamente el 8BitDo y DualSense.

## 0.6.13-beta-candidate (9 octubre 2026)

Candidata aislada desde 1e784b2 (0.6.10). No instalada ni validada en consola.

- Atiende el candidato EIR vacío antes del final de la ventana cuando hay tiempo y no hay otro enlace en curso; lo conserva mientras el enlace esté ocupado y limpia candidatos al cancelar/reiniciar una búsqueda.
- Evita iniciar otra Inquiry en el mismo poll que ejecuta una conexión diferida.
- Rechaza resultados de descubrimiento con longitudes, recuentos, modo de page scan o campos EIR incoherentes; cuenta los descartes. No cambia evstream ni intenta resincronizar bytes a ciegas.
- Recuerda en memoria los DualSense identificados durante la sesión, incluidos resultados posteriores sin modelo. Si se llena la lista de 16 exclusiones, bloquea nuevos emparejados Classic durante esa sesión.
- Elimina el barrido de desconexión de handles ajenos ante estado 0x0b; bloquea nuevos intentos a esa dirección hasta una petición explícita de búsqueda. Un enlace residual sin identidad verificada ya no se recupera automáticamente.
- Conserva transporte, pausa, LE, ACL, perfiles, SDP, autenticación y web de 0.6.10.

La detección de radio más rápida y la convivencia física con el DualSense/8BitDo siguen pendientes de pruebas de consola. Una búsqueda sin resultados no queda resuelta por esta versión.

## 0.6.10-beta (candidata experimental)

Sobre la 0.6.9, siguiendo la auditoría nº 4 (ideas.md §32). Se conserva la pausa que protege al DualSense.

1. **«Emparejar» coordinado con la pausa para el sistema.** Si se pulsa durante una pausa (un mando de la sesión anterior ha llamado a la consola al encenderse), la búsqueda **espera a que acabe la pausa y entonces tiene sus 60 s completos**. La web lo dice: «Otro mando acaba de llamar a la consola: la búsqueda empieza en N s…». Antes la búsqueda arrancaba a ciegas y se perdían de 17 a 28 s de ventana, más de 11 a 18 s hasta encontrar el mando (3 casos medidos en los logs). Durante la espera no se inicia ninguna pausa nueva.
2. **Tiempo hasta encontrar el mando:** el log dice `found gamepad … after N ms of search`, medido desde el inicio real de la búsqueda (no desde que se pulsó), para separar la detección del tiempo que tarda luego en conectar.
3. **Corrección de un fallo de la 0.6.9:** si un DualSense contestaba primero cortado (sin datos, sin modelo) y después completo, se reconocía y se excluía, pero la llamada pendiente seguía programada y se hacía a los 20 s. Ahora se cancela (`pending call to … cancelled: it is a DualSense`).

## 0.6.9-beta

Sobre la 0.6.8 (la marcada como correcta), solo para el emparejado:

1. **Un resultado de búsqueda cortado no se usa para llamar al mando.** Si el resultado extendido de un mando llega sin ningún dato (el resto de sus 255 bytes se lo llevó otro lector del chip), se espera hasta 20 s a otro resultado completo del mismo mando antes de llamarlo; pasado ese tiempo se le llama igualmente (hay mandos que no envían datos). En los logs guardados, las llamadas que siguieron a un resultado vacío fallaron 3 de 3 veces; con resultado completo, 14 de 29.
2. **Registro de la llamada:** el log escribe el modo de repetición de page scan, el desfase de reloj y los 14 bytes del resultado con los que se llama (`call data: …`), para ver si en las ejecuciones malas son anómalos.

No cambia nada de la fase conectada.

## 0.6.8-beta

**Vuelta a la base 0.6.0** (la publicada y estable para el usuario), con solo tres cambios demostrados o pedidos. Las 0.6.1–0.6.7 se descartan como base (rama `web-watchdog`): la 0.6.1 hizo que la pausa para el sistema congelara los mandos de AnyPad.

1. **Escaneo LE en pausa** mientras se llama o se prepara un mando Classic (de la 0.6.5). En consola, el DS4 pasó de no responder en 15 s a responder en 0,03–0,27 s, 4 de 4 veces.
2. **Datos ACL por el endpoint USB 0x02 desde el principio** (de la 0.6.7). El 0x01 de este chip ejecuta como orden HCI lo que se le escribe. Verificado en consola: ninguna orden basura, y unos 5 s menos por emparejado.
3. **Sesión limpia en cada ejecución** (de la 0.6.4, pedido por el usuario): `pads.db` se borra al arrancar y al parar.

Además, la primera línea del log lleva el commit (`build …`). Todo lo demás es igual que en la 0.6.0.

## 0.5.5-beta

- Primera beta. Base: 0.4.7-alpha, que encuentra el DS4 en la consola; la 0.5.4 (= 0.4.7 + botón «Pulsar PS») se confirmó que funciona y pasa a beta.
- El botón **«Pulsar PS»** (azul) se mueve a «Mandos emparejados», a la izquierda de «Olvidar», y aparece en los mandos conectados. Solo hace falta con mandos que no tienen botón PS (los que no son de PlayStation).
- Textos de la web y del README: al pulsarlo la consola muestra un mensaje del sistema que se refiere al DualSense (recuperar la prioridad); con un mando que no es de PlayStation la prioridad vuelve al DualSense, y para devolver el control al mando emparejado hay que volver al menú y pulsar «Pulsar PS» otra vez.
- Cambio quirúrgico: solo `web/index.html` (y la versión); la búsqueda y el Bluetooth no se tocan.

## 0.5.4-alpha

- Parte de la 0.4.7 (la que encuentra el DS4 en la consola del usuario) y añade SOLO el botón «Pulsar PS» por mando y los hasta 4 mandos. Se descartan los cambios de la búsqueda de mandos de 0.5.1/0.5.2 (vigilante de 14 s, cancelar al arrancar/parar, esperar ante 0x0c), porque con la 0.5.2 el DS4 dejó de encontrarse y con la 0.4.7 sí.
- Esos cambios pueden volver a probarse de uno en uno, solo si el usuario lo pide.

## 0.5.3-alpha

- Se ELIMINA el botón «Reiniciar Bluetooth» y el `HCI_Reset` (0.5.2): al probarlo en la consola el usuario se quedó sin DualSense; reiniciar el controlador del chip es peligroso. No se vuelve a añadir.
- Contiene lo de 0.5.0 (botón «Pulsar PS» por mando, hasta 4 mandos) y 0.5.1 (la búsqueda no se atasca, limpieza de búsqueda y enlaces huérfanos al arrancar).

## 0.5.1-alpha

- La búsqueda de mandos ya no se queda atascada: si el aviso de "fin de búsqueda" del chip se pierde, a los 14 s se cancela y se busca otra vez (antes quedaba parada hasta 45 s, y el mando "no aparecía").
- Al arrancar se cancela cualquier búsqueda que una ejecución anterior cortada dejara en el chip; si el chip contesta "comando no permitido" (0x0c) se espera a que acabe en vez de repetir la orden decenas de veces; al parar AnyPad no se deja ninguna búsqueda en marcha.
- Un aviso de fin de búsqueda dañado (longitud errónea) ya no se toma por bueno.
- Test nuevo `test_inquiry_watchdog` (probado con mutación).

## 0.5.0-alpha

- Base: 0.4.7-alpha (se descarta el cambio de Select como PS de la 0.4.8).
- Botón **«Pulsar PS»** en la tarjeta de cada mando del menú web: equivale a mantener pulsado PS en ese mando (envía PS al mando virtual durante 700 ms). Sirve para activar uno u otro mando como principal. Nuevo `POST /api/ps?slot=N`.
- Hasta 4 mandos a la vez (límite de `HOST_MAX_PADS`), documentado; la web explica cómo nombrar los usuarios de la consola según el mando.
- README: recomendación de nombrar el usuario como el mando, varios mandos, "igual en cualquier consola", cuenta de X @elmonomalvad0.

## 0.4.7-alpha

- Texto corregido sobre el usuario creado: se juega con el usuario principal y se pulsa PS un momento para usar el mando del usuario creado (notificación de arranque, web, README, notas de la release).
- README: se indica que se puede añadir al autoload; los mandos de otras consolas se presentan como "soportados para usar en la PS5, aún sin probar".

## 0.4.6-alpha

- Aviso de arranque y página web: hay que crear un usuario nuevo en la consola y asignarle el mando cuando lo detecte; con el usuario del DualSense los juegos no ven el mando. README actualizado.
- Nombre: desde esta versión los ELF se llaman `-alpha` en vez de `-experimental`.
- El log registra al arrancar el firmware de la consola (`console: firmware 10.01`) y los procesos que no son del sistema (otros payloads en marcha), para comparar consolas y firmwares.
- La web: el botón «Salir» solo cierra la página (AnyPad sigue funcionando); apagar AnyPad queda como botón «avanzado» aparte.
- Nuevo `COMPATIBILITY.md` (castellano e inglés): solo el DualShock 4 está verificado en consola.

## 0.4.5-experimental

- Base: 0.4.1 (se retiran de HEAD los cambios de 0.4.2/0.4.3; siguen en el historial) + `bind_user` de 0.4.4.
- Nuevo: si al paginar un mando el chip contesta "la conexión ya existe" (0x0b), se cierran los enlaces huérfanos de una ejecución anterior y se vuelve a buscar. Causa de que "se corrompa" tras relanzar el ELF sin parar antes.

## 0.4.4-experimental

- Construida sobre la **0.4.1** (la que encuentra el DS4), no sobre 0.4.2/0.4.3, más un único cambio: `/data/anypad/bind_user` (opcional) decide a qué usuario se da el mando virtual: `other` (un usuario con sesión que no sea el principal) o un id en hexadecimal. Sin el fichero todo es igual que la 0.4.1. Para probar si un juego lanzado por ese otro usuario recibe el mando.
- El log `users:` por fin se imprime cuando cambia algo (antes un detector erróneo no lo mostraba).

## 0.4.3-experimental

- Diagnóstico: cada 10 s, si llegó algo, el log muestra `usb: stats: N pieces, M events, D dropped, ACL K; types xx:n ...` (tipos de evento HCI). Sirve para ver si el chip responde durante la búsqueda cuando "no encuentra el mando" (0x02 = resultado de inquiry, 0x22 con RSSI, 0x2f extendido, 0x03 conexión completa, 0x0f estado de comando).

## 0.4.2-experimental

- Reintentos de paginación al mando (hasta 3) si falla la conexión durante el emparejamiento.
- El DS4 recibe antes su luz azul: el reintento de la secuencia de arranque pasa de 800 a 250 ms (hasta 12 rondas).
- Se quita de la web el aviso "Menú en la consola" (era una obviedad).
- Reensamblado de eventos: una pieza corta cierra el evento (evita que una pieza perdida estropee el siguiente). Ver HANDOFF.md.

## 0.4.1-experimental

- Prueba 6 (0.4.0): el DS4 se encuentra, conecta, se empareja y cifra el enlace, pero la escritura ACL por el endpoint 0x01 nunca termina ('ACL out still busy'), por lo que L2CAP no recibe respuesta. Si no termina en 0,5 s, se para y se usa el segundo bulk OUT de la función (0x02), y se registra 'usb: first ACL packet written on endpoint ...'.
- Se quita el atajo de mando (no se podía leer el DualSense desde un payload): el menú se abre desde Contenido multimedia o por la web.

## 0.4.0-experimental

- Acceso en "Contenido multimedia" (ANYP00001, "AnyPad", icono del mando naranja) que abre
  `http://127.0.0.1:8095/` en el navegador, con el mismo mecanismo que Payload Manager
  (`/user/app/ANYP00001/sce_sys/{param.json,icon0.png}` + `sceAppInstUtilAppInstallTitleDir`).
  Se instala al arrancar; `/data/anypad/no_icon` lo evita y `/data/anypad/remove_icon` lo desinstala.
  Icono: `assets/icon0.png` (512x512 PNG con transparencia, negro bajo los píxeles transparentes),
  generado con `tools/make-icon.py` y comprobado con `tools/check-icon.py`.
- Combinación de la consola por defecto: **L2 + R2 + OPTIONS** (1,5 s).
- Búsqueda de mandos con trazas ('inquiry: started / controller answered status / class ... ignored / finished')
  porque en la prueba 0.3.4 "Emparejar" no dejaba rastro alguno.

## 0.3.4-experimental (3 October 2026)

Made after the fourth run on a console (0.3.3), which dumped the Bluetooth chip's
USB configuration for the first time and explained the "silent" chip.

- **Fixed: the Bluetooth chip was answering and the answer was thrown away.** It
  answered `Read Buffer Size` with a valid *Command Complete* event
  (`0e 0b 01 05 10 00 fd 03 f0 08 00 08 00`) on endpoint `0x81`, which the code,
  following an assumed layout, treated as the ACL pipe. On this console
  (PS5 fat, firmware 10.01) the layout is the standard one: **events `0x81`
  (interrupt), ACL in `0x82`, ACL out `0x01`.** The endpoints are now read from the
  chip's own configuration descriptor (`usb_desc.c`, tested against the real
  dump in `tests/fixtures/`), and commands go to the interface of the chosen
  function. Events, which arrive in 16-byte pieces, are reassembled
  (`evstream.c`).
- **Fixed: the menu answered `{"error":"busy"}` after pressing the Bluetooth
  button.** Each Bluetooth try blocked the program for ~5 s without serving the
  web, the browser piled up connections and the 4 slots filled. The host now calls
  an idle hook whenever it waits, which serves the web; 8 slots.
- **Fixed: the console hotkey could not open the pad** (`scePadOpen` answered
  `0x809b0081`, `USER_NOT_LOGIN`). The user was taken from the foreground; now it
  is picked from the signed-in users (the initial one if signed in, then the
  foreground one, then the first), looked for again every 5 s if there was none
  or the system says it is not signed in. Virtual pads use the same choice.
- Logging: the pad-open and hotkey lines are written when the answer changes, not
  every 5 s.

## 0.3.3-experimental (3 October 2026)

Made after the third run on a console (0.3.2): the menu worked, the console
hotkey did not, and the Bluetooth chip still did not answer. See PRUEBAS.md.

- **Fixed: the console hotkey could not read the user's pad.** `scePadGetHandle`
  answered `0x80920008` (NO_HANDLE): a payload has no handle for the user's pad
  until it opens it. The pad is now opened the normal way (`scePadOpen`, as any
  application does; at most one try every 5 s, logged) and closed on exit.
- **Fixed: a stale instance lock survived a reboot.** The lock held only a
  process id, which means nothing after a reboot, so a leftover lock could look
  like a running copy. It now also holds the console's boot time, and a lock from
  another boot, or whose process is gone, is taken over (`lock.c`, `test_lock`).
- **More Bluetooth diagnosis.** The `diag:` lines now also dump the device's
  whole configuration descriptor (every interface and endpoint), the kernel driver
  holding each interface, the device's name and state, and the first read that
  finished (which endpoint, how many bytes, which bytes). Read-only; nothing is
  sent to or taken from the chip for this.
- The 0.3.2 result that shaped this: the credentials are **not** what stops the
  chip from answering (same silence with the original ones), and only one read
  finished in five seconds, with no event in it.

## 0.3.2-experimental (3 October 2026)

Made after the second run on a console (see PRUEBAS.md): the virtual-pad
start-up now passed, but the Bluetooth chip did not answer, and the menu never
appeared because it was started after the Bluetooth.

- **The menu starts first.** The web page is up before anything that can fail, so
  it is there to explain a failure. If the virtual pads or the Bluetooth are not
  available, AnyPad PS5 **stays running** in a reduced mode (menu, log, stop
  and a *Reintentar Bluetooth* button) instead of exiting.
- **The menu opens on the console with a button combination:** hold
  **L1 + R1 + touchpad for 1.5 s** on the DualSense and the console's browser
  opens `http://127.0.0.1:8095/` (`sceSystemServiceLaunchWebBrowser`, as Payload
  Manager does). Configurable in `/data/anypad/config.ini` (`menu_combo`,
  `menu_hold_ms`). The menu says whether the user's pad can be read and why not.
- **The start-up notification says where the menu is and how to open it:** the
  console's address (`http://<ip>:8095/`) and the combination, in Spanish.
- **Bluetooth diagnosis.** When the chip does not answer, the transport logs
  what it knows (`diag:` lines): the USB device, how many reads finished and with
  which status, how many events and ACL packets were seen, the first events in
  hexadecimal, and the last command sent.
- **Credentials experiment.** The first two Bluetooth tries are made with the
  raised credentials; if the chip is silent, they are put back and three more
  tries are made. The log says which worked. Virtual pads raise them again when
  needed. (Whether the raised credentials stop the chip from answering is not
  known.)
- After 5 failed tries (bounded) it stops trying by itself; the menu's *Reintentar
  Bluetooth* button tries again. Losing the Bluetooth later no longer stops the
  program: it reports it and offers the same button.
- The pairing key combination (L1+R1+Square) is gone; pair from the menu or with
  the `/data/anypad/pair` file.
- `host_poll(NULL)` does nothing; the store/web code tolerates a missing host.
- Tests: `test_config` (config, hotkey, local address), degraded-mode and
  hotkey fields in `test_web`, credentials restore in `test_vpad`, the dead
  controller in `test_host`.

## 0.3.1-experimental (3 October 2026)

First build after the first run on a console.

- **Fixed: it stopped at start-up.** `scePadSetProcessPrivilege` was called
  before `scePadInit` and its failure was fatal. On the console it answers
  `0x80920005` (`SCE_PAD_ERROR_NOT_INITIALIZED`) when called first. Now
  `scePadInit` goes first and a privilege answer other than 0 is only a
  warning. Details in
  [PRUEBAS.md](PRUEBAS.md#registro-de-pruebas-en-consola).
- Renamed from "padhost" to **AnyPad PS5**. The data folder is now
  `/data/anypad` (log `anypad.log`, flags `pair` and `stop`). The old
  `/data/padhost` is not used any more and can be deleted by hand.
- The store file's magic is now `ANYPAD02`.
- Web page on port 8095 (no PIN, by choice), with checks that need none.
- ELFs are named with their version.
- New test `test_vpad`, which runs the console's start-up code against
  stand-ins for its libraries; it fails on the 0.3.0 order.

## 0.3.0-experimental (3 October 2026)

First build tried on a console (PS5 fat, firmware 10.01, launched from a USB
drive with Payload Manager). SHA-256 `ffd592fb7b9d01a984f6b9ed615f1bb058fa82223f76f3334badc408a785e237`
for the build made on 2 October; the one run was a later 0.3 build from the same
source. It stopped at start-up (see 0.3.1) without touching Bluetooth.

Before this: 0.1 and 0.2 were never built as ELFs.
