# Bring-up checklist

Steps for a freshly assembled UniversalFlasher board.

## 1. First flash

1. Hold **SW100** (BOOTSEL) and plug in USB on CN100.
2. The board mounts as `RPI-RP2`. Copy `universalflasher.uf2` onto it.
3. The board reboots. D104 (power) and D107 (USB configured) light up.

If `RPI-RP2` doesn't appear, check the USB data lines and the 12 MHz crystal (X100). You can also reach the RP2040 over SWD on J100 with an external probe.

## 2. USB enumeration

The host should see one device, `2e8a:000c` "UniversalFlasher (CMSIS-DAP)", with a CMSIS-DAP v2 interface and two serial ports.

- macOS: `ls /dev/cu.usbmodem*` shows two ports.
- Linux: `lsusb -v -d 2e8a:000c` and `ls /dev/ttyACM*`.
- Windows: two COM ports appear in Device Manager, and the CMSIS-DAP interface binds to WinUSB without needing Zadig.

## 3. ESP32 (J101, CDC 0)

1. Connect the target to J101 (3.3 V logic, common GND).
2. Run `esptool --port <CDC 0> chip-id`. It should report the chip type and MAC.
3. Run `esptool --port <CDC 0> -b 921600 flash-id`. This exercises the baud rate switch.
4. Flash a firmware image and open the port in a serial monitor. The target must not reset when the monitor opens.

If step 2 can't connect, test the reset path on its own. Hold SW102 (BOOT), tap SW101 (EN), and run `esptool --before no-reset chip-id`.

## 4. SWD (J103)

1. Connect the target to J103. The target must be powered, and J103 pin 1 must see its 3.3 V rail.
2. Run `openocd -f interface/cmsis-dap.cfg -f target/<family>.cfg`. OpenOCD should find the DAP and the target.
3. Halt the target, read memory, then flash and verify an image.
4. Run `reset halt` to check the nRESET line (J103 pin 10).

D111 lights while a debugger is connected. D112 lights while the target is running.

## 5. Log UART (J102, CDC 1)

1. Short TXD and RXD on J102 (pins 2 and 3), open CDC 1 in a terminal and type. The characters should echo back.
2. With a real target, check data in both directions. If the target uses flow control, also check that RTS/CTS throttle at high baud rates.
