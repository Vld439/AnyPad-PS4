# Working on AnyPad PS4 with an AI assistant

This guide is written for an AI assistant (Claude Code, Codex CLI, Gemini CLI or any other with a terminal) and for the person who runs it. Its purpose: connect to a running AnyPad, watch what it does while a controller connects, and adapt it to a new controller or improve compatibility, with changes that can be tested and shared.

If you are the person: give the assistant this file as the first instruction, together with the prompt in section 8.

## 1. What you need

- A jailbroken PS4 that runs AnyPad (see the README), on the same local network as the computer where the assistant runs.
- On that computer: a terminal the assistant can use, `curl`, `make`, a C compiler, and the [ps4-payload-sdk](https://github.com/ps4-payload-dev/sdk) (`make ps4 PS4_PAYLOAD_SDK=...`).
- A way to send the new ELF to your payload loader (`make send PS4_HOST=<ip> PS4_PORT=<port>`, or however your loader takes it).

A chat in a web browser cannot reach your console: the assistant must be an agent with a terminal on your network. AnyPad never connects to the internet.

## 2. Rules that are not negotiable

1. **Leave Sony's controllers and the console's Bluetooth to the system.** The console keeps the DualShock 4, the DualSense and its own Bluetooth radio for itself. Do not try to read them, take them over, or get around the console's controller authentication. Wireless controllers go through the network controller (a phone does the Bluetooth).
2. **Do not change how the working controllers are read.** New code goes in a new profile, or a path chosen by the controller's vendor and product id, never in the common USB flow.
3. **One change per version, and test it before the next.** If something gets worse, go back to the last version that worked.
4. **Stop AnyPad before loading a new ELF** (`tools/live.sh stop`): a second copy refuses to start.
5. **Some payload loaders take one connection only.** Do not probe the loader's port: connect to it only to send the ELF, and ask the person to re-enable the loader if needed.
6. **Only your own console and your own network.** Never scan or connect to other people's devices.
7. **No secrets in issues or logs you share.** Do not paste tokens or passwords. Review a log before sharing it: it has the id of the console's users.
8. **The page has no PIN.** Anyone on the network can read the log and stop AnyPad while it runs. Stop it when you are done.

## 3. Connecting

AnyPad shows a notification when it starts with the console's address, and serves its page on port 8095. `tools/live.sh` wraps the API:

```
export ANYPAD_IP=192.168.x.y        # the console
tools/live.sh state                   # JSON: version, network controller, pads
tools/live.sh watch                   # the log in real time, saved in ./logs/
tools/live.sh log                     # the end of the log
tools/live.sh ps <n>                  # hold PS on pad n (1-4)
tools/live.sh stop                    # stop AnyPad
```

The same API, if you prefer `curl` (changes need the header `X-AnyPad: 1`, and the `Host` must be an IP address):

| Request | Does |
| --- | --- |
| `GET /api/state` | JSON: `version`, `lang`, `net` (`url`, `clients`), `pads` (live ones: `slot`, `label`, sticks, triggers, `buttons`) |
| `GET /api/live?from=N` | The log lines from number N on. The first line is `#next M`: ask from M next time. If `M` is smaller than what you had, AnyPad was restarted |
| `GET /api/log` | The tail of the log file |
| `GET /api/devs` | The device nodes in `/dev` (for diagnosis) |
| `POST /api/ps?slot=N` | Hold PS on pad N (1-4) |
| `POST /api/stop` | Stop AnyPad |

The network controller listens on port 8096: `GET /` is its page, which opens one WebSocket per controller the browser sees (each one is a client, and so a virtual pad, of its own). A WebSocket on that port (`/ws?n=<name>`: the controller's name, shown in the menu) takes 10-byte binary frames, or 21 with the page's touch area (`buttons` as a little-endian `uint32` with the `PAD_*` bits of `src/pad.h`, then `lx ly rx ry l2 r2`, one byte each, sticks centred at 128; the touch area adds a flags byte, bit 0 and 1 for fingers 0 and 1 down, then per finger its contact id, x and y as little-endian `uint16`, in touchpad units 0..1919 by 0..941).

Files on the console, if you can reach them (FTP): `/data/anypad/anypad.log` (and `.log.1`), `/data/anypad/maps/VVVV_PPPP.map` (a mapping for a controller with no profile; see `maps/EXAMPLE.map`), and an empty file `/data/anypad/stop` stops AnyPad.

## 4. Reading the log

Every line starts with the milliseconds since AnyPad started. The first line of a run is the version and the build: check it first, it must be the one you meant to test. Then, when a USB controller is plugged in:

| Line | Meaning |
| --- | --- |
| `usb_pad: usb:X.Y interfaces: 0.0=03/00/00 ...` | The device's interfaces (class/subclass/protocol). `ff/5d/01` Xbox 360, `ff/47/d0` Xbox One, `03` HID, `01/..` audio |
| `usb_pad: usb:X.Y (VVVV:PPPP): HID, interface 0, in 0x84 out 0x3, profile Switch Pro, slot 3` | The controller was recognised: how it is read, its endpoints, its profile and its slot |
| `usb_pad: usb:X.Y (054c:....): Sony pad, left to the console` | A Sony controller: not touched |
| `usb_pad: usb:X.Y: interface N is HID but not a gamepad` | A keyboard, a mouse or the like: not touched |
| `usb_pad: usb:X.Y: wake-up done (...)` / `power-on sent` | The controller was woken up (Switch, Xbox One) |
| `usb_pad: usb:X.Y speaks, virtual pad on slot N` | It sent its first report: a virtual pad is asked for |
| `slot N: AddDevice ...`, `slot N: device 0x... ready, owner left to the console` | The virtual DualShock 4 exists; the console asks who uses it |
| `slot N: virtual pad not identified (0 candidates) ...`, `asking again in 2 s` | The console did not announce the device; AnyPad tries again |
| `usb_pad: usb:X.Y stopped answering` | Unplugged, or it went silent |
| `netpad: client on slot N`, `netpad: slot N live, driven by a network client` | A phone or PC connected through the network controller |

## 5. How AnyPad is organised

`src/usb_pad.c` (the USB controllers, through `libSceUsbd`), `src/netpad.c` (the network controller), `src/profiles.c` (one profile per family of controllers), `src/generic.c` (any other HID gamepad), `src/ps4_vpad.c` (the virtual DualShock 4), `src/web.c` and `web/index.html` (the menu), `src/ps4_main.c` (start, the main loop, stopping). `tools/sce_stubs/` has the link stubs for `libSceUsbd` and `libSceMbus`.

Facts that cost time to learn:

- The PS4 has no `/dev/ugen*` nodes for userland: USB goes through `libSceUsbd`, a libusb under another name. System libraries are imported at link time (stubs); `dlopen` does not resolve them on this firmware.
- A system driver holding a controller's interface makes the console see it as a headset or a keyboard: AnyPad detaches it first.
- The virtual pad is device type 1, created with the controller's first report (a silent one would leave the console asking who uses a pad that cannot answer). It is identified from the kernel log line `SCE_MBUS_EVENT_DEVICE_ADDED [DeviceId:...][type:1]`; ids whose low byte is 0 are real pads re-registering. It is not bound to a user: `sceMbusBindDeviceWithUserId` faults on the PS4.
- The Switch Pro over USB needs its handshake (`80 02`, `80 03`, `80 02`, `80 04`) and a report every 500 ms, or it drops off within seconds.
- The data endpoints of Sony's controllers are refused to a payload (`0x802400ff`) even when the control endpoint answers: the console keeps them.

## 6. Adding a controller

1. **Get a log** of the controller being plugged in, with `tools/live.sh watch`. Note its vendor and product id and its interfaces.
2. **Try the generic profile first.** If it is HID and has no profile, it may already work. If a button or axis is wrong, a mapping file in `/data/anypad/maps/VVVV_PPPP.map` fixes it without code.
3. **If it needs its own profile** (a wake-up sequence, its own report layout, rumble or lights): add an entry in `src/profiles.c`. A profile is an id table, a `parse` function that turns a report into buttons, sticks and triggers, and, when needed, `init_report` (wake-up steps), `build_output` (rumble and lights) and `needs_full`. `src/profiles.h` documents each field. Read an existing profile of a similar family first.
4. **Add a test** in `tests/test_profiles.c` with a real report of the controller. A test must fail without your change.
5. **Run `make test` and `make fuzz`.** All must pass.
6. **Build the ELF (`make ps4`), stop AnyPad, load it and test with the real controller**, watching the log. One change per version.
7. **Share it** (section 7).

## 7. Sharing the result

Open an [issue](https://github.com/Vld439/AnyPad-PS4/issues) with:

- the controller (brand, model, vendor and product id), the console firmware, the jailbreak and the AnyPad version;
- what did not work, and the relevant log lines (reviewed first);
- what you changed and why, and how you tested it (`make test` and the real console);
- if an AI helped you, say so, and let it write the explanation of the controller's behaviour and of the change.

AnyPad PS4 is a port of [AnyPad PS5](https://codeberg.org/elmonomalva0/Any-Pad-ps5) by elmonomalvad0, under GPL-3.0-or-later: keep the notices, distribute changes under the same license, and please mention the original project.

## 8. Prompt to start with

Copy this to the assistant:

> You are going to help me adapt AnyPad PS4 to a new controller. Read `AI-WORKFLOW.md` and `README.md` in this repository completely before doing anything, and follow the rules of section 2. My PS4 is at `<ip>` and runs AnyPad (the version is in the first line of the log). Start with `tools/live.sh state` and then `tools/live.sh watch`. I will plug the controller in when you tell me. Read the log, explain what happens, propose one change, test it with `make test`, build the ELF and tell me how to load it. At the end, write the explanation I will post in an issue, saying that an AI helped.
