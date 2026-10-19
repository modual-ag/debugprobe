# RP2040 debug probe with ESP32 flashing

Firmware for an RP2040-based programmer that flashes and debugs ESP32 and SWD (e.g. STM32) targets over a
single USB cable.

It is a fork of Raspberry Pi's [debugprobe](https://github.com/raspberrypi/debugprobe) (v2.3.1), extended with a
second UART bridge, ESP32 auto-reset and a USB reset interface.

## What the host sees

The probe enumerates as one composite USB device (VID:PID `2e8a:000c`) with these interfaces:

| Interface | Function | Used for |
| --- | --- | --- |
| 0 | CMSIS-DAP v2 (WinUSB on Windows, no driver needed) | OpenOCD, pyOCD, probe-rs → SWD targets |
| 1 + 2 | CDC 0, ESP32 port | `esptool` / `idf.py` → ESP32 |
| 3 + 4 | CDC 1, log UART | Serial terminal → any UART target |
| 5 | Reset | `picotool` reboot into BOOTSEL |

The interface numbers are fixed: the udev rules and the Windows `MI_01` / `MI_03` port names depend on them.

| Host | CDC 0 (ESP32) | CDC 1 (log UART) |
| --- | --- | --- |
| macOS | `/dev/cu.usbmodem<serial>2` | `/dev/cu.usbmodem<serial>4` |
| Linux, with the udev rules | `/dev/probe-esp32` | `/dev/probe-log` |
| Linux, without | `/dev/ttyACM*` | `/dev/ttyACM*` |
| Windows | COM port on `MI_01` | COM port on `MI_03` |

On macOS, [macos/ufport](macos/ufport) looks a port up by its interface name, so scripts don't depend on the
USB socket. Run it without arguments to list every attached probe, or with `esp` or `log` to print one port.
With several probes attached, set `UF_SERIAL` to the serial number of the one to use.

On Linux, install the rules in [udev/](udev/) for the stable names and for debugger access without root:

```bash
sudo cp udev/*.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Pin map

Target I/O is 3.3 V only. The full pin map is in [include/](include/).

| Function | Signal | GPIO |
| --- | --- | --- |
| SWD | SWCLK | 12 |
| SWD | SWDIO out / SWDIO in | 14 / 13 |
| SWD | nRESET (open-drain) | 10 |
| SWD | Target voltage sense (ADC3) | 29 |
| ESP32 UART | TX / RX | 0 / 1 |
| ESP32 UART | DTR / RTS (active low, to the EN/IO0 reset circuit) | 8 / 9 |
| Log UART | TX / RX | 4 / 5 |
| Log UART | CTS / RTS | 6 / 7 |
| LED | USB configured | 2 |
| LED | ESP32 UART activity | 3 |
| LED | Log UART TX / RX activity | 17 / 18 |
| LED | Debugger connected / target running | 15 / 16 |

## Usage

### ESP32 (CDC 0)

DTR and RTS drive EN and IO0 through the usual cross-coupled transistor pair, the same way a CP2102/CH340 dev
board does. `esptool` and `idf.py` reset the chip into the bootloader on their own, with their default reset
settings:

```bash
esptool --port /dev/probe-esp32 chip-id
esptool --port /dev/probe-esp32 -b 921600 write-flash 0x0 firmware.bin
idf.py -p /dev/probe-esp32 flash monitor
```

Opening the port in a plain serial terminal doesn't reset the target: it asserts DTR and RTS together, which
leaves both EN and IO0 released. `idf.py monitor` resets the target on purpose when it starts; add `--no-reset`
to attach to a running target.

### SWD (CMSIS-DAP)

```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
pyocd list
probe-rs list
```

Use the target config that matches your MCU. The CMSIS-DAP interface drives nRESET.

### Log UART (CDC 1)

A plain serial port with hardware RTS/CTS flow control. The bridge only runs while the port is open (DTR
asserted). An unconnected CTS input is pulled to the active level, so flow control can be left unwired.

```bash
picocom -b 115200 /dev/probe-log
```

### AutoBaud (CDC 0)

Set the ESP32 port to the custom baud rate `9728` (`0x2600`). The bridge then measures the incoming data and
follows the target's baud rate. Setting any other baud rate turns AutoBaud off.

## Building

| Tool | Version | Notes |
| --- | --- | --- |
| CMake | 3.13 or newer | |
| `arm-none-eabi` toolchain with newlib | | the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads); Homebrew's `arm-none-eabi-gcc` lacks newlib |
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) | **2.3.0 or newer** | found through `PICO_SDK_PATH` |
| `picotool` | matching the SDK | optional, for flashing without the BOOTSEL button |

Set up the Pico SDK once:

```bash
git clone https://github.com/raspberrypi/pico-sdk.git --branch 2.3.0 ~/pico-sdk
git -C ~/pico-sdk submodule update --init
export PICO_SDK_PATH=~/pico-sdk
```

Then build:

```bash
git submodule update --init --recursive
cmake -B build
cmake --build build -j
```

The `.uf2` image lands in `build/`. The firmware version embedded in it is the exact git tag, or the short
commit hash when `HEAD` is not tagged.

## Updating the probe firmware

With `picotool`, no button needed. The probe reboots into BOOTSEL through the reset interface, loads the image
and starts it:

```bash
picotool load -f -x build/*.uf2
```

Without `picotool`, hold BOOTSEL while plugging in USB. The probe mounts as a drive called `RPI-RP2`; copy the
`.uf2` onto it and it reboots into the new firmware.

## Repository layout

| Path | Contents |
| --- | --- |
| [boards/](boards/) | Pico SDK board definition |
| [include/](include/) | Pin map and probe configuration |
| [src/cdc_uart.c](src/cdc_uart.c) | The two USB-UART bridges, DTR/RTS auto-reset |
| [src/usb_descriptors.c](src/usb_descriptors.c) | USB interfaces (CMSIS-DAP, 2× CDC, reset) |
| [src/autobaud.c](src/autobaud.c) | AutoBaud on CDC 0 |
| [src/vtarget.c](src/vtarget.c) | Target voltage sense |
| [macos/ufport](macos/ufport) | Port lookup by interface name on macOS |
| [udev/](udev/) | Stable port names and permissions on Linux |
| `CMSIS_DAP/`, `freertos/` | Vendored CMSIS-DAP sources and the FreeRTOS kernel submodule |

## License

MIT, as upstream. See [LICENSE](LICENSE).
