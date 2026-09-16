/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
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

#include <pico/stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"
#include "autobaud.h"

#include "probe_config.h"
#include "cdc_uart.h"

/*
 * Generalised from the original single-UART cdc_uart.c into an array of
 * bridges, one per CDC interface. See board_universalflasher_config.h for
 * why bridge 0 (ESP32) uses software DTR/RTS and bridge 1 (log) uses
 * hardware flow control - the two are mutually exclusive on a single UART.
 */

#define DEBOUNCE_MS 40
/* Max 1 FIFO worth of data */
#define BRIDGE_BUF_SIZE 32
/* UART needs to preempt USB, or characters get lost */
#define UART_TASK_PRIO (tskIDLE_PRIORITY + 3)

typedef struct {
    uint8_t itf;
    uart_inst_t *uart;
    uint tx_pin, rx_pin;
    uint32_t default_baud;

    bool hwfc;
    uint cts_pin, rts_pin; /* valid when hwfc */

    /* Software modem-control outputs; -1 if unused. */
    int dtr_gpio, rts_gpio;
    /*
     * Classic CDC-ACM semantics suspend UART polling while DTR is low (port
     * "not open"). The ESP32 bridge instead uses DTR/RTS purely as the
     * esptool reset control lines (S4.3 of the spec) - esptool pulses DTR
     * low as part of the reset sequence while still needing bytes to flow,
     * so that bridge must NOT suspend on DTR low.
     */
    bool suspend_on_dtr_low;
    bool autobaud_capable;

    /* -1 if unused; tx_led/rx_led may alias the same GPIO (combined activity LED) */
    int tx_led, rx_led;

    const char *task_name;

    /* Runtime state */
    TaskHandle_t task_handle;
    TickType_t last_wake, interval;
    volatile TickType_t break_expiry;
    volatile bool timed_break;
    uint debounce_ticks;
    volatile uint tx_led_debounce;
    uint rx_led_debounce;
    int was_connected;
    uint cdc_tx_oe;
    uint8_t tx_buf[BRIDGE_BUF_SIZE];
    uint8_t rx_buf[BRIDGE_BUF_SIZE];
} cdc_uart_bridge_t;

static cdc_uart_bridge_t bridges[CDC_UART_NUM_BRIDGES] = {
    [CDC_UART_BRIDGE_ESP32] = {
        .itf = CDC_UART_BRIDGE_ESP32,
        .uart = PROBE_UART0_INTERFACE,
        .tx_pin = PROBE_UART0_TX,
        .rx_pin = PROBE_UART0_RX,
        .default_baud = PROBE_UART0_BAUDRATE,
        .hwfc = false,
        .dtr_gpio = PROBE_UART0_DTR,
        .rts_gpio = PROBE_UART0_RTS,
        .suspend_on_dtr_low = false,
        .autobaud_capable = true,
        .tx_led = PROBE_UART0_TX_LED,
        .rx_led = PROBE_UART0_RX_LED,
        .task_name = "UART-ESP32",
        .interval = 100,
        .debounce_ticks = 5,
    },
    [CDC_UART_BRIDGE_LOG] = {
        .itf = CDC_UART_BRIDGE_LOG,
        .uart = PROBE_UART1_INTERFACE,
        .tx_pin = PROBE_UART1_TX,
        .rx_pin = PROBE_UART1_RX,
        .default_baud = PROBE_UART1_BAUDRATE,
        .hwfc = true,
        .cts_pin = PROBE_UART1_CTS,
        .rts_pin = PROBE_UART1_RTS,
        .dtr_gpio = -1,
        .rts_gpio = -1,
        .suspend_on_dtr_low = true,
        .autobaud_capable = false,
        .tx_led = PROBE_UART1_TX_LED,
        .rx_led = PROBE_UART1_RX_LED,
        .task_name = "UART-LOG",
        .interval = 100,
        .debounce_ticks = 5,
    },
};

/* Combined-LED-safe update: writes each debounce state once, OR'd together
 * when tx_led/rx_led alias the same GPIO (the ESP32 bridge's single
 * "activity" LED), so TX and RX don't clobber each other's indication. */
static void bridge_update_leds(cdc_uart_bridge_t *b, bool tx_active, bool rx_active)
{
    if (tx_active)
        b->tx_led_debounce = b->debounce_ticks;
    else if (b->tx_led_debounce)
        b->tx_led_debounce--;

    if (rx_active)
        b->rx_led_debounce = b->debounce_ticks;
    else if (b->rx_led_debounce)
        b->rx_led_debounce--;

    bool tx_on = b->tx_led_debounce > 0;
    bool rx_on = b->rx_led_debounce > 0;

    if (b->tx_led < 0 && b->rx_led < 0)
        return;

    if (b->tx_led == b->rx_led) {
        gpio_put(b->tx_led, tx_on || rx_on);
        return;
    }
    if (b->tx_led >= 0)
        gpio_put(b->tx_led, tx_on);
    if (b->rx_led >= 0)
        gpio_put(b->rx_led, rx_on);
}

static void bridge_leds_off(cdc_uart_bridge_t *b)
{
    b->tx_led_debounce = 0;
    b->rx_led_debounce = 0;
    if (b->tx_led >= 0)
        gpio_put(b->tx_led, 0);
    if (b->rx_led >= 0 && b->rx_led != b->tx_led)
        gpio_put(b->rx_led, 0);
}

void cdc_uart_init(void)
{
    for (int i = 0; i < CDC_UART_NUM_BRIDGES; i++) {
        cdc_uart_bridge_t *b = &bridges[i];

        gpio_set_function(b->tx_pin, GPIO_FUNC_UART);
        gpio_set_function(b->rx_pin, GPIO_FUNC_UART);
        gpio_set_pulls(b->tx_pin, 1, 0);
        gpio_set_pulls(b->rx_pin, 1, 0);
        uart_init(b->uart, b->default_baud);

        if (b->tx_led >= 0) {
            gpio_init(b->tx_led);
            gpio_set_dir(b->tx_led, GPIO_OUT);
        }
        if (b->rx_led >= 0 && b->rx_led != b->tx_led) {
            gpio_init(b->rx_led);
            gpio_set_dir(b->rx_led, GPIO_OUT);
        }

        if (b->hwfc) {
            /* HWFC implies that hardware flow control is implemented and the
             * UART operates in "full-duplex" mode (See USB CDC PSTN120 6.3.12).
             * Default to pulling in the active direction, so an unconnected CTS
             * behaves the same as if CTS were not enabled. */
            gpio_set_pulls(b->cts_pin, 0, 1);
            gpio_set_function(b->rts_pin, GPIO_FUNC_UART);
            gpio_set_function(b->cts_pin, GPIO_FUNC_UART);
            uart_set_hw_flow(b->uart, true, true);
        } else if (b->rts_gpio >= 0) {
            gpio_init(b->rts_gpio);
            gpio_set_dir(b->rts_gpio, GPIO_OUT);
            gpio_put(b->rts_gpio, 1);
        }

        if (b->dtr_gpio >= 0) {
            gpio_init(b->dtr_gpio);
            gpio_set_dir(b->dtr_gpio, GPIO_OUT);
            gpio_put(b->dtr_gpio, 1);
        }
    }
}

static bool bridge_task(cdc_uart_bridge_t *b)
{
    uint rx_len = 0;
    bool keep_alive = false;
    bool rx_active = false, tx_active = false;

    /* Consume uart fifo regardless even if not connected */
    while (uart_is_readable(b->uart) && (rx_len < sizeof(b->rx_buf))) {
        b->rx_buf[rx_len++] = uart_getc(b->uart);
    }

    if (tud_cdc_n_connected(b->itf)) {
        b->was_connected = 1;
        int written = 0;
        /* Implicit overflow if we don't write all the bytes to the host.
         * Also throw away bytes if we can't write... */
        if (rx_len) {
            rx_active = true;
            written = MIN(tud_cdc_n_write_available(b->itf), rx_len);
            if (rx_len > written)
                b->cdc_tx_oe++;

            if (written > 0) {
                tud_cdc_n_write(b->itf, b->rx_buf, written);
                tud_cdc_n_write_flush(b->itf);
            }
        }

        /* Reading from a firehose and writing to a FIFO. */
        size_t watermark = MIN(tud_cdc_n_available(b->itf), sizeof(b->tx_buf));
        if (watermark > 0) {
            size_t tx_len;
            tx_active = true;
            /* Batch up to half a FIFO of data - don't clog up on RX */
            watermark = MIN(watermark, 16);
            tx_len = tud_cdc_n_read(b->itf, b->tx_buf, watermark);
            uart_write_blocking(b->uart, b->tx_buf, tx_len);
        }

        bridge_update_leds(b, tx_active, rx_active);

        /* Pending break handling */
        if (b->timed_break) {
            if (((int)b->break_expiry - (int)xTaskGetTickCount()) < 0) {
                b->timed_break = false;
                uart_set_break(b->uart, false);
                b->tx_led_debounce = 0;
            } else {
                keep_alive = true;
            }
        }
    } else if (b->was_connected) {
        tud_cdc_n_write_clear(b->itf);
        uart_set_break(b->uart, false);
        b->timed_break = false;
        b->was_connected = 0;
        bridge_leds_off(b);
        b->cdc_tx_oe = 0;
    }
    return keep_alive;
}

static void bridge_set_baudrate(cdc_uart_bridge_t *b, uint32_t baudrate)
{
    /* Set the tick thread interval to the amount of time it takes to
     * fill up half a FIFO. Millis is too coarse for integer divide.
     */
    uint32_t micros = (1000 * 1000 * 16 * 10) / MAX(baudrate, 1);
    b->interval = MAX(1, micros / ((1000 * 1000) / configTICK_RATE_HZ));
    b->debounce_ticks = MAX(1, configTICK_RATE_HZ / (b->interval * DEBOUNCE_MS));
    probe_info("[%s] New baud rate %ld micros %ld interval %lu\n",
               b->task_name, baudrate, micros, b->interval);
    uart_deinit(b->uart);
    tud_cdc_n_write_clear(b->itf);
    tud_cdc_n_read_flush(b->itf);

    uart_init(b->uart, baudrate);
}

static void bridge_thread(void *ptr)
{
    cdc_uart_bridge_t *b = (cdc_uart_bridge_t *)ptr;
    BaudInfo_t baud_info;
    BaseType_t delayed;
    bool keep_alive;

    b->last_wake = xTaskGetTickCount();
    /* Threaded with a polling interval that scales according to linerate */
    while (1) {
        keep_alive = bridge_task(b);
        if (!keep_alive) {
            delayed = xTaskDelayUntil(&b->last_wake, b->interval);
            if (delayed == pdFALSE)
                b->last_wake = xTaskGetTickCount();
            if (b->autobaud_capable && autobaud_running) {
                /* Receive baud information from autobaud thread */
                if (xQueueReceive(baudQueue, &baud_info, 0) == pdTRUE) {
                    bridge_set_baudrate(b, baud_info.baud);
                    /* Assume 8N1 */
                    uart_set_format(b->uart, 8, 1, UART_PARITY_NONE);
                }
            }
        }
    }
}

void cdc_uart_start_tasks(void)
{
    for (int i = 0; i < CDC_UART_NUM_BRIDGES; i++) {
        cdc_uart_bridge_t *b = &bridges[i];
        xTaskCreate(bridge_thread, b->task_name, configMINIMAL_STACK_SIZE,
                    b, UART_TASK_PRIO, &b->task_handle);
#if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(b->task_handle, (1 << 0));
#endif
    }
}

void cdc_uart_suspend_tasks(void)
{
    for (int i = 0; i < CDC_UART_NUM_BRIDGES; i++)
        vTaskSuspend(bridges[i].task_handle);
}

void cdc_uart_resume_tasks(void)
{
    for (int i = 0; i < CDC_UART_NUM_BRIDGES; i++)
        vTaskResume(bridges[i].task_handle);
}

void cdc_uart_delete_tasks(void)
{
    for (int i = 0; i < CDC_UART_NUM_BRIDGES; i++)
        vTaskDelete(bridges[i].task_handle);
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *line_coding)
{
    cdc_uart_bridge_t *b = &bridges[itf];

    if (line_coding->bit_rate == MAGIC_BAUD) {
        if (b->autobaud_capable && !autobaud_running)
            autobaud_start();
        return;
    } else if (b->autobaud_capable && autobaud_running) {
        autobaud_wait_stop();
    }
    uart_parity_t parity;
    uint data_bits, stop_bits;

    /* Modifying state, so park the thread before changing it. */
    if (tud_cdc_n_connected(itf))
        vTaskSuspend(b->task_handle);

    bridge_set_baudrate(b, line_coding->bit_rate);

    switch (line_coding->parity) {
    case CDC_LINE_CODING_PARITY_ODD:
        parity = UART_PARITY_ODD;
        break;
    case CDC_LINE_CODING_PARITY_EVEN:
        parity = UART_PARITY_EVEN;
        break;
    default:
        probe_info("invalid parity setting %u\n", line_coding->parity);
        /* fallthrough */
    case CDC_LINE_CODING_PARITY_NONE:
        parity = UART_PARITY_NONE;
        break;
    }

    switch (line_coding->data_bits) {
    case 5:
    case 6:
    case 7:
    case 8:
        data_bits = line_coding->data_bits;
        break;
    default:
        probe_info("invalid data bits setting: %u\n", line_coding->data_bits);
        data_bits = 8;
        break;
    }

    /* The PL011 only supports 1 or 2 stop bits. 1.5 stop bits is translated to 2,
     * which is safer than the alternative. */
    switch (line_coding->stop_bits) {
    case CDC_LINE_CONDING_STOP_BITS_1_5:
    case CDC_LINE_CONDING_STOP_BITS_2:
        stop_bits = 2;
        break;
    default:
        probe_info("invalid stop bits setting: %u\n", line_coding->stop_bits);
        /* fallthrough */
    case CDC_LINE_CONDING_STOP_BITS_1:
        stop_bits = 1;
        break;
    }

    uart_set_format(b->uart, data_bits, stop_bits, parity);
    /* Windows likes to arbitrarily set/get line coding after dtr/rts changes, so
     * don't resume if we shouldn't */
    if (tud_cdc_n_connected(itf))
        vTaskResume(b->task_handle);
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    cdc_uart_bridge_t *b = &bridges[itf];

    /* Active-low, mimicking a CP2102's DTR#/RTS# pins (see board config /
     * spec S4.3 for the ESP32 bridge's cross-coupled reset circuit). */
    if (b->rts_gpio >= 0)
        gpio_put(b->rts_gpio, !rts);
    if (b->dtr_gpio >= 0)
        gpio_put(b->dtr_gpio, !dtr);

    if (!b->suspend_on_dtr_low)
        return;

    /* CDC drivers use linestate as a bodge to activate/deactivate the interface.
     * Resume our UART polling on activate, stop on deactivate */
    if (!dtr) {
        vTaskSuspend(b->task_handle);
        bridge_leds_off(b);
    } else {
        vTaskResume(b->task_handle);
    }
}

void tud_cdc_send_break_cb(uint8_t itf, uint16_t wValue)
{
    cdc_uart_bridge_t *b = &bridges[itf];

    switch (wValue) {
    case 0:
        uart_set_break(b->uart, false);
        b->timed_break = false;
        b->tx_led_debounce = 0;
        break;
    case 0xffff:
        uart_set_break(b->uart, true);
        b->timed_break = false;
        if (b->tx_led >= 0)
            gpio_put(b->tx_led, 1);
        b->tx_led_debounce = 1 << 30;
        break;
    default:
        uart_set_break(b->uart, true);
        b->timed_break = true;
        if (b->tx_led >= 0)
            gpio_put(b->tx_led, 1);
        b->tx_led_debounce = 1 << 30;
        b->break_expiry = xTaskGetTickCount() + (wValue * (configTICK_RATE_HZ / 1000));
        break;
    }
}
