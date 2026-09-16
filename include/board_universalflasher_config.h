/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef BOARD_UNIVERSALFLASHER_H_
#define BOARD_UNIVERSALFLASHER_H_

/*
 * UniversalFlasher: RP2040 multi-target flasher/debugger.
 * Pin numbers below are verified against RP2040_Flash.SchDoc's netlist
 * (SC0914(13), U100) - not against the original spec doc, which turned out
 * to have the SWDI/SWDIO assignment backwards. Targets are 3.3V only: the
 * SWD buffer (U101, SN74AUP1T17) runs from the board's own +3V3, not from a
 * VTARGET-tracked VCCB - there is no multi-voltage level translation on this
 * board despite earlier planning docs describing one.
 */

// Buffered SWD: SWCLK/SWDIO driven by the RP2040, SWDI sampled back from the
// SN74AUP1T17 buffer's output pin.
#define PROBE_IO_SWDI
#define PROBE_CDC_UART

// ---------------------------------------------------------------------------
// SWD (target: STM32) - DO NOT MOVE, and DO NOT re-derive this from
// board_example_config.h's PROBE_IO_SWDI comment block - it disagrees with
// the actually-shipped board_debug_probe_config.h and is wrong for this
// hardware. Confirmed against the real schematic netlist:
//   GPIO12 (DBG_CLK)     -> R101 100R -> target SWCLK           = PROBE_PIN_SWCLK
//   GPIO13 (DBG_DIO_BUF) <- U101 (SN74AUP1T17) output           = PROBE_PIN_SWDI  (read-back only, never an output)
//   GPIO14 (DBG_DIO)     -> R103 100R -> SWDIO net -> target,   = PROBE_PIN_SWDIO (driven + pulled up)
//                           and -> R107 100R -> U101 input
// The PROBE_PIN_OFFSET+{0,1,2} arithmetic itself doesn't have to follow any
// particular order (probe.pio addresses SWDIO/SWDI by macro, not by fixed
// offset position - verified against sw_dp_pio.c/probe.c/probe.pio); what's
// fixed is which physical GPIO plays which SWD role, and that must match the
// schematic exactly, which is what's encoded below.
// ---------------------------------------------------------------------------
#define PROBE_SM 0
#define PROBE_PIN_OFFSET 12
#define PROBE_PIN_SWCLK (PROBE_PIN_OFFSET + 0) // GPIO12 - DBG_CLK
#define PROBE_PIN_SWDI  (PROBE_PIN_OFFSET + 1) // GPIO13 - DBG_DIO_BUF, read-back only
#define PROBE_PIN_SWDIO (PROBE_PIN_OFFSET + 2) // GPIO14 - DBG_DIO, to target, pulled up

// STM32 nRST, open-drain via small N-FET (bidirectional OD, active-low)
#define PROBE_PIN_RESET 10

// ---------------------------------------------------------------------------
// UART bridge 0 (CDC 0): ESP32 flash/console UART with esptool-style
// DTR/RTS -> EN/IO0 auto-reset via the cross-coupled NPN pair. Software
// (ACM line-state) modem control - no hardware flow control on this UART, so
// the CDC line-state callback is free to drive GPIO8/9 as plain GPIOs.
// ---------------------------------------------------------------------------
#define PROBE_UART0_TX 0
#define PROBE_UART0_RX 1
#define PROBE_UART0_INTERFACE uart0
#define PROBE_UART0_BAUDRATE 115200
// nDTR / nRTS to the cross-coupled reset transistors, active-low (see cdc_uart.c)
#define PROBE_UART0_DTR 8
#define PROBE_UART0_RTS 9
// Single combined "ESP UART activity" LED (TX and RX share GPIO3)
#define PROBE_UART0_TX_LED 3
#define PROBE_UART0_RX_LED 3

// ---------------------------------------------------------------------------
// UART bridge 1 (CDC 1): log/comms UART with real hardware flow control.
// Mutually exclusive with ACM-driven RTS (see spec S3) - this is exactly why
// it is a second, independent UART/CDC pair rather than sharing bridge 0.
// ---------------------------------------------------------------------------
#define PROBE_UART1_TX 4
#define PROBE_UART1_RX 5
#define PROBE_UART1_INTERFACE uart1
#define PROBE_UART1_BAUDRATE 115200
#define PROBE_UART1_HWFC
#define PROBE_UART1_CTS 6
#define PROBE_UART1_RTS 7
#define PROBE_UART1_TX_LED 17
#define PROBE_UART1_RX_LED 18

// ---------------------------------------------------------------------------
// Remaining LEDs
// ---------------------------------------------------------------------------
#define PROBE_USB_CONNECTED_LED 2
#define PROBE_DAP_CONNECTED_LED 15
#define PROBE_DAP_RUNNING_LED 16

// VTARGET sense: GPIO29 = ADC3, fed by a 10k/10k divider from the SWD
// header's Vref pin (R126/R127) - confirmed against the schematic netlist.
// Reading x2 (after adc_gpio_init(29), see probe_config.c) gives target
// rail millivolts.
#define PROBE_VTARGET_ADC_PIN 29
#define PROBE_VTARGET_ADC_INPUT 3

#define PROBE_PRODUCT_STRING "UniversalFlasher"

#endif
