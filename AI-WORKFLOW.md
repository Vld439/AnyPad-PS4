# Working on AnyPad PS5 with an AI assistant

This guide is written for an AI assistant (Claude Code, Codex CLI, Gemini CLI or any other with a terminal) and for the person who runs it. Its purpose: connect to a running AnyPad, watch what it does while a controller connects, and adapt it to a new controller or improve compatibility, with changes that can be tested and shared.

If you are the person: give the assistant this file as the first instruction, together with the prompt in section 9.

## 1. What you need

- A jailbroken PS5 that runs AnyPad (see the README), on the same local network as the computer where the assistant runs.
- On that computer: a terminal the assistant can use, `curl`, `make`, a C compiler, and the [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) (the script `tools/build-elf-docker.sh` builds without installing it, using Docker).
- A way to put the new ELF where your payload loader reads it (a USB stick, or an FTP payload on the console). That part depends on your setup and is not part of AnyPad.

A chat in a web browser cannot reach your console: the assistant must be an agent with a terminal on your network. AnyPad never connects to the internet.

## 2. Rules that are not negotiable

1. **Never send an HCI Reset (opcode `0C03`) and never restart the Bluetooth chip.** The chip is shared with the console's own driver. Resetting it breaks the system's Bluetooth, including the DualSense. The page refuses `0C03`; do not look for another way.
2. **Do not change how the DualShock 4 and the 8BitDo connect.** They are the reference that works. New code goes in a new profile or a new path chosen by the controller's vendor and product id, never in the common search, call and link flow.
3. **One change per version, and test it before the next.** If something gets worse, go back to the last version that worked.
4. **Do not switch off the DualSense while AnyPad runs** and do not ask the person to; it can stay blinking blue until the console is restarted. The person should also not be asked to run many tests: read the log first.
5. **Only your own console and your own network.** Never scan or connect to other people's devices.
6. **No secrets in issues or logs you share.** Do not paste tokens or passwords. Review a log before sharing it: it has the Bluetooth addresses of the controllers and the id of the console user. `/data/anypad/pads.db` holds link keys; never share it (AnyPad deletes it at every start and stop).
7. **The page has no PIN.** Anyone on the network can read the log and send commands while AnyPad runs. Stop AnyPad when you are done.

## 3. Connecting

AnyPad shows a notification when it starts with the console's address, and serves its page on port 8095. `tools/live.sh` wraps the API:

```
export ANYPAD_IP=192.168.x.y        # the console
tools/live.sh state                   # JSON: version, chip, bluetooth state, pads
tools/live.sh watch                   # the log in real time, saved in ./logs/
tools/live.sh settings                # the settings that can be changed live
tools/live.sh set <name> <value>      # change one
tools/live.sh hci <op> [hex]          # one HCI command (never 0C03)
tools/live.sh pair                    # press "Pair" (60 s)
tools/live.sh stop                    # stop AnyPad
```

The same API, if you prefer `curl` (changes need the header `X-AnyPad: 1`, and the `Host` must be an IP address):

| Request | Does |
| --- | --- |
| `GET /api/state` | JSON: `version`, `running`, `pairing` (seconds left), `pause`, `chip`, `bluetooth`, `reason`, `pads` (connected, with their buttons and sticks), `paired` |
| `GET /api/live?from=N` | The log lines from number N on. The first line is `#next M`: ask from M next time. If `M` is smaller than what you had, AnyPad was restarted |
| `GET /api/log` | The tail of the log file |
| `GET /api/tune` | The live settings, as JSON |
| `POST /api/tune?name=value` | Change a live setting |
| `POST /api/hci?op=HEX&p=HEX` | One HCI command; the reply parameters come back as hex |
| `POST /api/pair` | Start a 60 s search |
| `POST /api/forget?addr=AA:BB:..` | Forget a controller |
| `POST /api/stop` | Stop AnyPad |

Live settings (`/api/tune`): `ps_light` (0/1: lighten the system's page scan while pairing), `ps_iv` and `ps_win` (interval and window of that lightened page scan, in 0.625 ms slots), `mask_conn` (0/1: leave the incoming-connection event out of the event mask), `inq_len` (inquiry length, in 1.28 s units), `idle_reads` (event reads kept pending while idle), `ev_len` (length asked of each event read), `trace` (0/1: trace the link events in the log).

Files on the console, if you can reach them: `/data/anypad/anypad.log` (and `.log.1`), `/data/anypad/maps/VVVV_PPPP.map` (a mapping for a controller with no profile; see `maps/EXAMPLE.map`), and an empty file `/data/anypad/stop` stops AnyPad.

## 4. Reading the log

Every line starts with the milliseconds since AnyPad started. The first line of a run is the version and the build. Check it first: it must be the version you meant to test. Then, in a normal pairing:

| Line | Meaning |
| --- | --- |
| `chip: MediaTek (0e8d:3603)` or `Marvell/NXP (1286:2059)` | The Bluetooth chip of this console. Only the first has been verified on a real console |
| `chip: page scan activity: ...` | How much the system listens for calls. The value `9c 02 4e 01` makes the console half deaf while searching; AnyPad lightens it while pairing |
| `inquiry: AA:BB:.. class 002508 eir name="..."` | A device answered the search |
| `found gamepad ...`, `connecting to pair` | AnyPad calls it |
| `paired, key stored`, `link encrypted` | The link is secure |
| `vendor 054c product 09cc` (or `has no profile, reading its HID descriptor`) | The controller was identified. Without a profile it goes to the generic one |
| `channel 0x11 open`, `channel 0x13 open` | The HID channels |
| `ready as <profile> in slot N` | Linked and its profile chosen |
| `first input (report 0x.., N bytes): virtual DualSense created` | It sent data: the console now sees a controller |
| `not in its full report yet; asking again every 3 s (reports seen: ...)` | The controller stays in a simple mode: its wake-up sequence is not working. The reports seen tell what it sends |
| `incoming connection from ...: not ours, left to the system` | Someone else's device (for example the DualSense) is calling the console; AnyPad steps aside |

## 5. How AnyPad is organised

See the table in the README. In short: `src/host.c` (Classic), `src/le.c` (LE), `src/profiles.c` (one profile per family of controllers), `src/generic.c` (any other HID gamepad), `src/hci_usb.c` (HCI over USB with the shared chip), `src/ps5_vpad.c` (the virtual DualSense), `src/web.c` and `web/index.html` (the page). `tests/sim.c` and `tests/sim_le.c` simulate a controller and the chip, so a change can be checked on a computer.

Facts that cost time to learn:

- The chip is shared with the system's driver: both read the same events. AnyPad answers what is its own and leaves the rest to the system.
- A controller that is switched on normally calls the console; AnyPad sees that call and steps aside for a moment so the system can answer. Searching and pairing a controller in pairing mode is the path that works every time.
- `pads.db` is deleted at every start: every run pairs the controllers again. Reconnection by themselves is an open problem (see section 8).
- The virtual DualSense is created with the first input of the controller, not before: one that stays silent would block the console's user screen.
- The console asks which user uses a new controller. The README asks for "Quick Play / Guest".

## 6. Adding a controller

1. **Get a log** of the controller pairing, with `tools/live.sh watch`. Note the vendor and product id (`vendor ... product ...`) or the name it announces.
2. **Try the generic profile first.** If the line says `has no profile, reading its HID descriptor`, the controller may already work. If a button or axis is wrong, a mapping file in `/data/anypad/maps/VVVV_PPPP.map` fixes it without code.
3. **If it needs its own profile** (it needs a wake-up sequence, has its own report layout, rumble or lights): add an entry in `src/profiles.c`. A profile is an id table, a `parse` function that turns a report into buttons, sticks and triggers, and, when needed, `init_report` (wake-up steps), `build_output` (rumble and lights) and `needs_full`. `src/profiles.h` documents each field. Read an existing profile of a similar family first (`switch_*` for Switch-like pads, `ds4_*` for DualShock-like ones, `xbox_*`).
4. **Teach the simulator** how that controller behaves (`tests/sim.h`, `sim_pad`: its reports, whether it needs waking, its PIN, and so on) and add a test in `tests/test_compat.c`. A test must fail without your change.
5. **Run `make test`.** All tests must pass. Also compare the fingerprint lines `METRIC kind=... hash=...` that `make test` prints for the DualShock 4 and the Switch Pro before and after your change: the hashes must be identical. If they differ, your change touched the common flow; move it into the new profile.
6. **Build the ELF, put it on the console and test with the real controller**, watching the log. One change per version.
7. **Report it** (section 7).

## 7. Sharing the result

Open an [issue](https://codeberg.org/elmonomalva0/Any-Pad-ps5/issues) with:

- the controller (brand, model, vendor and product id), the console model and firmware, the jailbreak, the chip line of the log and the AnyPad version;
- what did not work, and the relevant log lines (not the whole log if it is long, and reviewed first);
- what you changed and why, and how you tested it (simulator test and real console);
- if an AI helped you, say so, and let it write the explanation of the controller's behaviour and of the change.

The maintainer reviews each proposal and integrates the compatible ones in later versions. If you publish your own version, or include AnyPad in an emulator, a HEN, a payload or any other program, you are asked to mention **elmonomalvad0** and link the project. The GPL-3.0-or-later license requires keeping the notices and distributing changes under the same license.

## 8. Known open problems

- The DualShock 4 and the 8BitDo do not reconnect by themselves after being switched off: the DualShock 4 rejects the key AnyPad stored (authentication failure `0x05`) and the 8BitDo, when it calls the console, stays in a simple mode that ignores the wake-up sequence. Pairing them again in pairing mode always works.
- A DualSense switched off while AnyPad runs can stay blinking blue.
- Sometimes the chip hears nothing in a search ("deaf"); a console restart helps.
- Marvell/NXP consoles (`1286:2059`): supported in the code, never verified on a real console. A log would help a lot.
- Xbox controllers over Bluetooth LE are reported to work; the exact models are not identified yet.

## 9. Prompt to start with

Copy this to the assistant:

> You are going to help me adapt AnyPad PS5 to a new controller. Read `AI-WORKFLOW.md` and `README.md` in this repository completely before doing anything, and follow the rules of section 2. My PS5 is at `<ip>` and runs AnyPad (the version is in the first line of the log). Start with `tools/live.sh state` and then `tools/live.sh watch`. I will put the controller in pairing mode when you tell me. Read the log, explain what happens, propose one change, test it with `make test`, build the ELF and tell me how to load it. At the end, write the explanation I will post in an issue, saying that an AI helped.
