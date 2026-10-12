# UniversalFlasher firmware

Firmware for the UniversalFlasher board: an RP2040-based programmer that flashes and debugs ESP32 and SWD (e.g. STM32) targets over a single USB-C cable.

It is a fork of Raspberry Pi's [debugprobe](https://github.com/raspberrypi/debugprobe) (v2.3.1), extended with a second UART bridge and ESP32 auto-reset.

## What the host sees

The board enumerates as one composite USB device (VID:PID `2e8a:000c`) with three functions:

| Function | Appears as | Used for |
|---|---|---|
| CMSIS-DAP v2 | USB vendor interface (WinUSB on Windows, no driver needed) | OpenOCD, pyOCD, probe-rs → SWD targets on J103 |
| CDC 0 "ESP32 Flash Port" | Serial port | esptool / PlatformIO / Arduino → ESP32 on J101 |
| CDC 1 "Debug Log UART" | Serial port | Terminal → log UART on J102 |

On macOS, the ports appear as `/dev/cu.usbmodemXXXX2` (CDC 0) and `/dev/cu.usbmodemXXXX4` (CDC 1). On Linux they appear as `/dev/ttyACM*`. Install [udev/99-universalflasher.rules](udev/99-universalflasher.rules) to get the stable names `/dev/probe-esp32` and `/dev/probe-log`.

## Connectors

All target connectors are 2×5 1.27 mm headers. Target I/O is 3.3 V only.

**J101 – ESP32 (UART boot)**

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | +5 V (fused) | 2 | USB D+ (to CN101) |
| 3 | TXD → ESP32 RX | 4 | EN |
| 5 | GND | 6 | GND |
| 7 | BOOT (IO0) | 8 | RXD ← ESP32 TX |
| 9 | USB D− (to CN101) | 10 | +5 V (fused) |

Pins 2 and 9 pass through to the second USB-C connector (CN101), so the native USB of an ESP32-S3/C3/C6 can be used with a second cable.

**J102 – Log UART**

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | GND | 2 | TXD → target RX |
| 3 | RXD ← target TX | 4 | +5 V (fused) |
| 5 | RTS → target CTS | 6 | CTS ← target RTS |

**J103 – SWD** (standard ARM 10-pin Cortex debug pinout)

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | VTref (target 3.3 V) | 2 | SWDIO |
| 3 | GND | 4 | SWCLK |
| 5 | GND | 6 | SWO (not connected) |
| 9 | GND | 10 | nRESET |

**J100** (Tag-Connect TC2050) is SWD for the RP2040 itself. You can use it to recover the board with an external probe.

## Buttons and LEDs

| | Function |
|---|---|
| SW100 | RP2040 BOOTSEL (hold while plugging in USB to update the firmware) |
| SW101 | ESP32 EN (reset) |
| SW102 | ESP32 BOOT (IO0) |

| LED | GPIO | Meaning |
|---|---|---|
| D104 | – | 3.3 V power |
| D103 | – | USB overvoltage |
| D107 | 2 | USB configured by host |
| D108 | 3 | ESP32 UART activity |
| D109 / D110 | 18 / 17 | Log UART RX / TX activity |
| D111 | 15 | Debugger connected (CMSIS-DAP) |
| D112 | 16 | Target running (CMSIS-DAP) |

## Usage

### ESP32 (CDC 0)

The bridge drives EN and IO0 through the usual cross-coupled transistor pair, the same way a CP2102/CH340 dev board does. Standard tools reset the chip into the bootloader automatically:

```
esptool --port /dev/cu.usbmodemXXXX2 chip-id
esptool --port /dev/cu.usbmodemXXXX2 -b 921600 write-flash 0x0 firmware.bin
```

For PlatformIO, set `upload_port` and `monitor_port` to the CDC 0 port. No reset options are needed.

Opening the port in a serial monitor doesn't reset the target. Opening it asserts DTR and RTS together, which leaves both EN and IO0 released.

### SWD (CMSIS-DAP)

```
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
pyocd list
probe-rs list
```

Use the target config that matches your MCU. The CMSIS-DAP interface drives nRESET on J103 pin 10.

### Log UART (CDC 1)

The log UART is a plain serial port with hardware RTS/CTS flow control. The bridge only runs while the port is open (DTR asserted). An unconnected CTS input is pulled to the active level, so flow control can be left unwired.

### AutoBaud (CDC 0)

Set the ESP32 port to the custom baud rate `9728` (`0x2600`). The bridge then measures the incoming data and follows the target's baud rate. Setting any other baud rate turns AutoBaud off.

## Updating the board firmware

1. Hold **SW100** and plug in USB. The board mounts as a drive called `RPI-RP2`.
2. Copy `universalflasher.uf2` onto it. The board reboots into the new firmware.

## Building

You need CMake, an `arm-none-eabi` toolchain that includes newlib (the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads); Homebrew's `arm-none-eabi-gcc` is not enough), and [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.x.

```
git clone https://github.com/raspberrypi/pico-sdk.git --branch 2.1.1 ~/pico-sdk
git -C ~/pico-sdk submodule update --init
export PICO_SDK_PATH=~/pico-sdk

git submodule update --init --recursive
mkdir build && cd build
cmake ..
make -j8
```

The output is `build/universalflasher.uf2`.

## Repository layout

| Path | Contents |
|---|---|
| [include/board_universalflasher_config.h](include/board_universalflasher_config.h) | Pin map for this board |
| [src/cdc_uart.c](src/cdc_uart.c) | The two USB-UART bridges, DTR/RTS auto-reset |
| [src/usb_descriptors.c](src/usb_descriptors.c) | USB interfaces (CMSIS-DAP + 2× CDC) |
| [src/vtarget.c](src/vtarget.c) | Target voltage sense (J103 pin 1, ADC3) |
| [BRINGUP.md](BRINGUP.md) | Bring-up checklist for a new board |

## License

MIT, as upstream. See [LICENSE](LICENSE).
