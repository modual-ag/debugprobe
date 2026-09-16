# RP2040 Multi-Target Flasher & Debugger — Project Specification

**Status:** Hardware design approved to proceed. Firmware fork required (scoped below).
**Base firmware:** `raspberrypi/debugprobe` (master, ≥ v2.0.1)
**Reference hardware:** `red-scorp/rp2040-debugprobe` (4-layer KiCad, JLCPCB-ready)

This document is a self-contained handoff. It carries the verified facts, the pin map,
the firmware work breakdown, and the open questions. Anyone picking this up (human or
LLM) should be able to start from here without re-deriving the analysis.

---

## 1. Goal

A single programming board that connects to a host PC with **one USB-C cable** and
simultaneously provides:

1. **STM32 flash + debug** over native 4-wire SWD (CMSIS-DAP, full GDB breakpoints)
2. **ESP32 flash** over UART with automatic reset (`EN` / `IO0` driven by DTR/RTS)
3. **Log/comms UART** with hardware flow control (TX/RX/CTS/RTS), usable at the same
   time as either of the above
4. **Passive USB pass-through** on a second USB-C connector, for native USB-OTG /
   USB-JTAG debugging of ESP32-S3/C3/C6 (requires a second cable to the PC)

Constraint: one-time flash, one-time setup. No per-session reconfiguration, no button
presses, no reflashing between targets.

---

## 2. What the host PC sees

One composite USB device (VID `0x2E8A`) with **three** interfaces — **two** of which
are COM ports:

| Interface | Host-side appearance | Consumer |
|---|---|---|
| CMSIS-DAP v2 (vendor, WinUSB via MS OS 2.0 descriptors) | "CMSIS-DAP v2 Interface" — **not** a COM port | OpenOCD / pyOCD (STM32) |
| CDC 0 | COM port / `/dev/ttyACM*` | esptool (ESP32) |
| CDC 1 | COM port / `/dev/ttyACM*` | log terminal |

The STM32 path is **not** serial. OpenOCD speaks SWD over the bulk vendor interface
via libusb/WinUSB. No Zadig needed — the firmware already ships MS OS 2.0 descriptors.

The second USB-C connector is electrically passive and does not appear in this
enumeration. When a target is attached through it, the PC sees the target's own USB
device (e.g. ESP32-S3 = VID `0x303A`).

---

## 3. Verified facts (checked against upstream source, not assumed)

These corrected an earlier plan. Do not silently revert them.

| Claim | Verdict | Evidence |
|---|---|---|
| debugprobe exposes 2 CDC ports | **False.** One CDC only. | `usb_descriptors.c`; `cdc_uart.c` is written around a single `PROBE_UART_INTERFACE` |
| Config file is `src/board_debugprobe_config.h` | **False.** It's `include/board_debug_probe_config.h` | repo layout: four headers live in `include/` |
| Stock `debugprobe.uf2` matches GPIO2/3 SWD | **False.** That build uses GPIO12/13/14 | `board_debug_probe_config.h`: `PROBE_PIN_OFFSET 12` |
| HW flow control is supported | **True, but exclusive.** `PROBE_UART_HWFC` enables CTS/RTS **and disables** SET_LINE_STATE control of the RTS pin | upstream commit "cdc_uart: add CTS/RTS configuration options" |
| nRST is supported | **True, optional.** `PROBE_PIN_RESET`, active-low; not defined by default | `board_example_config.h`; v2.0 release note "Fix target reset functionality for pins 0 and 1" |
| Cross-coupled NPN auto-reset circuit (emitters on opposing control lines) | **Correct.** Standard NodeMCU/CP2102 topology | — |

**Consequence of row 4 (the critical one):** on a single UART you can have hardware
flow control *or* host-driven RTS-as-GPIO, never both. Therefore ESP32 flashing and the
flow-controlled log channel **must live on two different UARTs behind two different CDC
interfaces.** This is the reason the fork is mandatory.

---

## 4. Hardware architecture

### 4.1 SWD block — DO NOT MOVE

Keep `red-scorp`'s SWD block byte-for-byte, including the level translators. Reasons:

- The PIO program requires **consecutive GPIOs** starting at `PROBE_PIN_OFFSET`.
  Buffered mode: SWCLK = +0, SWDI = +1, SWDIO = +2. They cannot be scattered.
- The translators (74LVC1T45-class, VCCB from target VREF) are what make the probe
  work with 1.8 V / 2.5 V / 3.3 V targets. Required for the "universal" goal.
- Using the reference assignment means the known-good `PROBE_IO_SWDI` config path
  applies unchanged.

`SWDI` is a separate **read-back input** — the buffered design cannot read the bus on
the same pin it drives, so the translator's output is fed back to a distinct GPIO.

### 4.2 Pin map

| Signal | GPIO | Notes |
|---|---|---|
| SWCLK | 12 | reference, buffered. Fixed. |
| SWDI (read-back) | 13 | reference, buffered. Fixed. |
| SWDIO | 14 | reference, buffered. Fixed. |
| STM32 nRST | 10 | **open-drain via small N-FET** — STM32 NRST is bidirectional OD |
| ESP32 UART TX | 0 | uart0 |
| ESP32 UART RX | 1 | uart0 |
| nDTR (to reset circuit) | 8 | emulated DTR, active-low. **Not** EN — see §4.3 |
| nRTS (to reset circuit) | 9 | emulated RTS, active-low. **Not** IO0 — see §4.3 |
| Log UART TX | 4 | uart1 |
| Log UART RX | 5 | uart1 |
| Log UART CTS | 6 | uart1, **input** |
| Log UART RTS | 7 | uart1, **output** |
| LED: USB connected | 2 | |
| LED: ESP UART activity | 3 | |
| LED: DAP connected | 15 | |
| LED: DAP running | 16 | |
| LED: log UART TX | 17 | |
| LED: log UART RX | 18 | |
| VTARGET sense (optional) | 28 or 29 | must be ADC-capable |

Free afterwards: 11, 19–27.

**RP2040 UART pin-mux constraint** (UART pins are NOT freely assignable):

- uart0 — TX: 0/12/16/28 · RX: 1/13/17/29 · CTS: 2/14/18 · RTS: 3/15/19
- uart1 — TX: 4/8/20/24 · RX: 5/9/21/25 · CTS: 6/10/22/26 · RTS: 7/11/23/27

**Reference-board LED conflict:** `red-scorp` places LEDs on GPIO 2, 7, 8, 15, 16.
GPIO 7 (log RTS) and GPIO 8 (nDTR) collide. Those two LEDs must be relocated
in the schematic.

Header labelling: the probe's TX is an output → target RX. Probe RTS (output) →
target CTS. Probe CTS (input) ← target RTS. Silkscreen from the probe's point of view
and state the convention on the board.

### 4.3 ESP32 auto-reset circuit

Standard cross-coupled NPN pair (MMBT3904 or similar), ~12 kΩ base resistors.

**`EN` and `IO0` are transistor collector outputs going to the target. They are NOT
RP2040 pins and must never be wired to one.** The only RP2040 connections are the two
emulated modem-control lines, and each one lands in *two* places:

```
GPIO8 (nDTR) ──┬── 12k ── base   Q1        Q1 collector ── EN   → ESP32
               └───────── emitter Q2

GPIO9 (nRTS) ──┬── 12k ── base   Q2        Q2 collector ── IO0  → ESP32
               └───────── emitter Q1
```

Netlist, explicitly:

| Node | Connects to |
|---|---|
| GPIO8 (nDTR) | Q1 base (via 12 kΩ), Q2 emitter |
| GPIO9 (nRTS) | Q2 base (via 12 kΩ), Q1 emitter |
| Q1 collector | target `EN` |
| Q2 collector | target `IO0` |

**Polarity: drive both GPIOs active-low**, mimicking a CP2102 whose DTR#/RTS# pins go
low when the host asserts them. In `tud_cdc_line_state_cb` for CDC 0:

```c
gpio_put(PIN_NDTR, !dtr);
gpio_put(PIN_NRTS, !rts);
```

Resulting behaviour:

| host DTR | host RTS | GPIO8 | GPIO9 | `EN` | `IO0` |
|---|---|---|---|---|---|
| 0 | 0 | 1 | 1 | free | free |
| 0 | 1 | 1 | 0 | **LOW** (reset) | free |
| 1 | 0 | 0 | 1 | free | **LOW** (bootloader) |
| 1 | 1 | 0 | 0 | free | free |

The bottom row is the reason for the cross-coupled topology: a terminal program that
raises DTR and RTS together on port open will **not** reboot the target. A naive
two-GPIO design would. Together the middle two rows reproduce esptool's classic reset
sequence exactly.

Because polarity is set in firmware, it is not locked in at schematic time — if the
sense comes out backwards on the bench it is a `!` in one line, not a board rework.

Collector outputs are effectively open-collector: they can only pull low, so they
cannot fight the target's pull-ups. Do **not** add a capacitor on `EN` at the flasher
end — the EN cap belongs on the target board and a second one breaks esptool's
reset timing.

RP2040 GPIOs are push-pull 3.3 V, identical to a CP2102's DTR/RTS pins, so the
circuit works unmodified.

### 4.4 Power

- Board input: 5 V (USB VBUS) → on-board 3.3 V regulator.
- Translator VCCB from **VTARGET**: either sensed from the target connector
  (preferred, fully automatic) or a jumper selecting 3V3 / 1V8 / external.
- Optional DUT power output behind a jumper. Add a polyfuse or current-limited load
  switch, plus a series Schottky or a load switch with proper enable so an externally
  powered DUT cannot back-feed the board's 3V3 rail.

### 4.5 USB

- **5.1 kΩ CC1/CC2 pull-downs on BOTH USB-C receptacles.** Without them a C-to-C
  cable delivers nothing.
- Keep the two VBUS nets **separate** — do not tie them.
- The two grounds are common. The pass-through is **not** galvanically isolated;
  don't describe it as such.
- USBLC6-2SC6 ESD array placed close to the pass-through connector.
- Route D+/D− as a 90 Ω differential pair; keep the header stub short.
- BOOTSEL button + RUN/reset button for the RP2040 itself.
- Series resistors (22–33 Ω) on outgoing signal lines for ringing control.

---

## 5. Firmware fork — work breakdown

Repo: `git clone https://github.com/raspberrypi/debugprobe && git submodule update --init`
Build: `cmake -B build -DPICO_SDK_PATH=... && cmake --build build`

**Recommended toolchain: the official "Raspberry Pi Pico" VS Code extension**
(`raspberry-pi.raspberry-pi-pico` on the marketplace). It manages the SDK, ARM
toolchain, CMake, OpenOCD and GDB for you, and is officially supported on macOS
(Sonoma+), Linux (x64/arm64), Windows and Raspberry Pi OS — no platform-specific
tooling and nothing Windows-only anywhere in this chain. Team setup:

- macOS: `xcode-select --install` (installs Git + Tar)
- Linux: Python ≥3.10, Git, Tar on PATH; optionally install udev rules so OpenOCD/
  picotool don't need sudo
- Flashing your own firmware onto the board (bring-up, or a teammate reflashing
  later) needs no tool at all: hold BOOTSEL (or strap the BOOTSEL pin low at
  power-on, since this is a bare RP2040, not a Pico module) and the chip enumerates
  as a generic USB mass-storage drive on any OS. Drag the `.uf2` onto it.
- Debugging the STM32 target over SWD once the board is running also goes through
  this same extension (OpenOCD + Cortex-Debug/GDB), no separate install.

### Task 1 — Board config header
New `include/board_<name>_config.h` modelled on `board_debug_probe_config.h`.
Define `PROBE_IO_SWDI`, `PROBE_PIN_OFFSET 12`, `PROBE_PIN_RESET 10`, both UART
instances, `PROBE_UART_HWFC` **for uart1 only**, and the new LED pins. Add a matching
target in `CMakeLists.txt`.

### Task 2 — Second CDC interface
`tusb_config.h`: `CFG_TUD_CDC` 1 → 2.
`usb_descriptors.c`: add the second CDC interface pair, two endpoints, bump
`ITF_NUM_TOTAL` and `CONFIG_TOTAL_LEN`.

> **Highest-risk item.** The MS OS 2.0 / BOS descriptor references the vendor
> interface **by index** for WinUSB binding. Inserting CDC interfaces shifts that
> index. Get it wrong and CMSIS-DAP silently fails to bind on Windows while
> everything looks fine on Linux. Verify with USBView / `lsusb -v` before trusting it.

Also add distinct interface string descriptors ("ESP32 Flash Port", "Debug Log UART")
so Device Manager labels the two COM ports rather than showing two identical entries.

### Task 3 — Generalise `cdc_uart.c`
Currently hard-wired to one `PROBE_UART_INTERFACE`. Refactor to an array of two
instances, each with its own ring buffers, FreeRTOS task, `tud_cdc_n_*` calls and
line-coding callback.

### Task 4 — DTR/RTS → GPIO for ESP32 reset
Implement `tud_cdc_line_state_cb(itf, dtr, rts)` for **CDC 0 only**, driving GPIO8/9.
Confirm whether upstream already maps DTR (as opposed to only RTS) — if not, write it.
esptool needs both.

### Task 5 — Hardware flow control on uart1
`uart_set_hw_flow(uart1, true, true)` plus `gpio_set_function` for GPIO6/7.
`PROBE_UART_HWFC` on uart1 only; leave it off for uart0 so Task 4 keeps working.

### Task 6 — LEDs
Extend `led.c` for the additional indicators.

### Task 7 — Host-side stability
udev rule matching `ID_USB_INTERFACE_NUM` → stable `/dev/probe-esp32` and
`/dev/probe-log` symlinks.

---

## 6. Bench validation checklist (cannot be verified in simulation)

- [ ] Windows enumerates all three interfaces; CMSIS-DAP binds WinUSB without Zadig
- [ ] Both COM ports appear with correct labels; numbering stable across replug
- [ ] OpenOCD attaches to STM32 via `interface/cmsis-dap.cfg` + target cfg
- [ ] `reset_config srst_only` correctly drives GPIO10 through the FET
- [ ] esptool auto-reset works end-to-end — **most likely source of trouble.**
      DTR/RTS arrive as USB control transfers with ~1 ms latency. Usually fine
      (every native-USB ESP32-S2 board behaves this way) but some targets need
      `--before default_reset` tuning.
- [ ] Log UART flow control actually throttles at high baud
- [ ] SWD signal integrity at target speed; translators track VTARGET correctly
- [ ] Both COM ports usable simultaneously under load
- [ ] Pass-through port enumerates an ESP32-S3 correctly

---

## 7. Key BOM anchors

| Part | MPN | Note |
|---|---|---|
| MCU | Raspberry Pi **SC0914(13)** | = RP2040, 56-QFN 7×7. There is no "RP2040" MPN at distributors. ~0.61 CHF, 18-week factory lead time |
| QSPI flash | W25Q16JV / W25Q128JV | 8-USON or 8-SOIC |
| Crystal | 12 MHz, load cap per RP2040 hardware design guide | |
| ESD (pass-through) | USBLC6-2SC6 | |
| Level translators | 74LVC1T45-class, per reference design | |
| Reset transistors | 2 × MMBT3904 | |

---

## 8. Open questions / decisions not yet made

1. **`red-scorp` buffer topology** — does it use plain `PROBE_IO_SWDI` (separate
   read-back input) or `PROBE_IO_OEN` (GPIO-driven output-enable)? The firmware has
   both code paths and the board config must match. **Requires reading the schematic.**
2. VTARGET: auto-sense from the connector, or jumper-selected?
3. Does the ESP32 UART also need level translation, or is 3.3 V-direct acceptable?
4. Connector choice for the target headers (JST-SH like the reference, or 0.1" for
   bench flexibility?).
