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
 * UniversalFlasher (UniversalFlasher.SchDoc, rev B.0.01).
 * Targets are 3.3V only: the SWD read-back buffer (U101) runs from +3V3.
 */

#define PROBE_IO_SWDI
#define PROBE_CDC_UART

// SWD (J103). SWDIO is driven directly; the target's SWDIO is read back
// through the U101 buffer on a separate pin.
#define PROBE_SM 0
#define PROBE_PIN_OFFSET 12
#define PROBE_PIN_SWCLK (PROBE_PIN_OFFSET + 0) // GPIO12 DBG_CLK
#define PROBE_PIN_SWDI  (PROBE_PIN_OFFSET + 1) // GPIO13 DBG_DIO_BUF
#define PROBE_PIN_SWDIO (PROBE_PIN_OFFSET + 2) // GPIO14 DBG_DIO

// Target nRST via R104, driven open-drain
#define PROBE_PIN_RESET 10 // DBG_NRST

// CDC 0: ESP32 UART (J101) with DTR/RTS auto-reset via Q100/Q101
#define PROBE_UART0_TX 0 // DUT_TX
#define PROBE_UART0_RX 1 // DUT_RX
#define PROBE_UART0_INTERFACE uart0
#define PROBE_UART0_BAUDRATE 115200
#define PROBE_UART0_DTR 8 // DUT_DTR, active low
#define PROBE_UART0_RTS 9 // DUT_RTS, active low
#define PROBE_UART0_TX_LED 3 // LED_DUT_ACT, shared TX/RX activity
#define PROBE_UART0_RX_LED 3

// CDC 1: log UART (J102) with hardware flow control
#define PROBE_UART1_TX 4
#define PROBE_UART1_RX 5
#define PROBE_UART1_INTERFACE uart1
#define PROBE_UART1_BAUDRATE 115200
#define PROBE_UART1_HWFC
#define PROBE_UART1_CTS 6
#define PROBE_UART1_RTS 7
#define PROBE_UART1_TX_LED 17
#define PROBE_UART1_RX_LED 18

#define PROBE_USB_CONNECTED_LED 2
#define PROBE_DAP_CONNECTED_LED 15
#define PROBE_DAP_RUNNING_LED 16

// Target voltage sense: J103 pin 1 through the R122/R123 10k/10k divider
#define PROBE_VTARGET_ADC_PIN 29
#define PROBE_VTARGET_ADC_INPUT 3

#define PROBE_PRODUCT_STRING "UniversalFlasher (CMSIS-DAP)"

#endif
