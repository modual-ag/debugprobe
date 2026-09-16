# Bring-up: UniversalFlasher (RP2040_Flash.SchDoc)

What to do, in order, the moment the assembled board arrives. This assumes nothing has
been flashed yet and nothing is proven on real hardware — see the "cannot be proven in
software" list at the bottom before trusting anything beyond this sequence.

## 1. First flash (no debugger needed)

The RP2040 on this board is bare silicon (`SC0914(13)`), not a Pico module — there is no
pre-existing UF2 bootloader button behavior to rely on beyond BOOTSEL itself.

1. Hold the BOOTSEL button (SW102, tied to the RP2040's boot-select strap) while applying
   power, or while pressing the RUN/reset button (SW100).
2. The board enumerates as a plain USB mass-storage drive (`RPI-RP2` or similar).
3. Drag `universalflasher.uf2` onto it. It resets and boots the new firmware automatically.

## 2. What the host should see next

One composite USB device, VID `0x2E8A`:

| Interface | Appears as | For |
|---|---|---|
| CMSIS-DAP v2 (vendor, WinUSB) | Not a COM port | OpenOCD / pyOCD → STM32 SWD |
| CDC 0 ("ESP32 Flash Port") | COM port / `/dev/ttyACM*` | esptool → ESP32 |
| CDC 1 ("Debug Log UART") | COM port / `/dev/ttyACM*` | Log terminal |

Check the three USB status LEDs come up as expected (LED_USB_CON should light once the
host finishes enumeration — see the LED table in the PR/summary this file accompanies).

## 3. SWD sanity check (STM32)

With an STM32 target wired to the J103 SWD header:

```
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg   # or whatever target family
```

(swap the target cfg for your actual STM32 family). Confirm it attaches and can halt/read
memory. This is the single highest-risk item in the whole board — if it fails, check
GPIO13/14 continuity against the schematic before suspecting firmware, since a reversed
SWDIO/SWDI wiring error here is silent (probe enumerates fine, SWD just never responds).

## 4. ESP32 auto-reset check

With an ESP32 wired to J101:

```
esptool.py --port <CDC0 port> chip_id
```

If auto-reset doesn't drop the target into the bootloader, try `esptool.py --before
default_reset` or `--before no_reset` plus a manual BOOT/EN button press (SW101/SW102 on
the DUT side, not the RP2040's own BOOTSEL) to isolate whether it's a timing issue or a
polarity issue. Polarity is one `!` in `cdc_uart.c`'s `tud_cdc_line_state_cb`, not a
board respin, if it turns out inverted.

## 5. Log UART + flow control check

Open the CDC1 port at a working baud rate and confirm bytes flow both ways. If you have a
target that asserts CTS, confirm the RP2040 actually throttles instead of overrunning at
high baud (this cannot be verified without real target hardware asserting flow control).

## 6. VTARGET sanity check

No USB-visible readout exists yet (see the open question in the PR/summary about whether
you want one exposed). To sanity-check the ADC path on the bench, temporarily enable
`probe_info` in `probe_config.h` and add a print of `vtarget_read_mv()` — expect roughly
target-rail-voltage in mV with the target powered, and near 0 with it unpowered.

## 7. If the USB path is dead

The board exposes RP2040 SWD + RUN on a separate Tag-Connect (TC2050-IDC) footprint
(J100), independent of the onboard CMSIS-DAP circuitry. A second, external SWD probe can
recover/reflash the RP2040 through J100 even if the primary USB/CMSIS-DAP path is
non-functional.

---

## Cannot be proven in software — confirm these on the bench

- Windows enumerates all three interfaces and binds WinUSB to the CMSIS-DAP interface
  without Zadig.
- Both COM ports appear with correct labels and stable numbering across replug.
- OpenOCD actually attaches over `interface/cmsis-dap.cfg` to a real STM32.
- esptool auto-reset works end-to-end (see §4) — most likely source of bring-up trouble.
- Flow control on the log UART actually throttles at high baud under real load.
- SWD signal integrity at full target clock speed.
- Both COM ports usable simultaneously under sustained load without starving each other.
- The passive pass-through port (CN101) correctly enumerates a native ESP32-S3/C3/C6.
