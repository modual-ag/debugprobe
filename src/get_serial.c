/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Federico Zuccardi Merli
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

#include <stdint.h>
#include "pico.h"
#include "pico/unique_id.h"
#include "get_serial.h"

/*
 * C string for iSerialNumber in USB Device Descriptor: the 64-bit flash unique ID folded to 32 bits, as 8 hex
 * digits + terminating NUL. macOS only names a CDC port after the serial number when it is at most 8 characters
 * long (/dev/cu.usbmodem<serial><n>); a longer one falls back to the USB location, so the port name would change
 * with every socket or hub.
 */
#define USB_SERIAL_BYTES 4
char usb_serial[USB_SERIAL_BYTES * 2 + 1];

static pico_unique_board_id_t uID;

void usb_serial_init(void)
{
    uint8_t folded[USB_SERIAL_BYTES];

    pico_get_unique_board_id(&uID);

    for (int i = 0; i < USB_SERIAL_BYTES; i++)
        folded[i] = uID.id[i] ^ uID.id[i + USB_SERIAL_BYTES];

    for (int i = 0; i < USB_SERIAL_BYTES * 2; i++)
    {
        /* High nibble first, so the string reads in memory order */
        uint8_t nibble = (folded[i / 2] >> ((i % 2) ? 0 : 4)) & 0x0F;
        usb_serial[i] = nibble < 10 ? nibble + '0' : nibble + 'A' - 10;
    }
    usb_serial[USB_SERIAL_BYTES * 2] = '\0';
}
