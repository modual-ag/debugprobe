#include <hardware/adc.h>

#include "probe_config.h"
#include "vtarget.h"

// GPIO29/ADC3, 10k/10k divider off the SWD header's Vref pin - see
// board_universalflasher_config.h. adc_gpio_init() must run before the first
// conversion: it disables the pin's digital input buffer and pulls, both of
// which otherwise bias the reading (RP2040 pads reset with a ~50-80k
// pull-down enabled, which in parallel with the divider's lower leg would
// read several percent low).
void vtarget_init(void)
{
    adc_init();
    adc_gpio_init(PROBE_VTARGET_ADC_PIN);
}

uint32_t vtarget_read_mv(void)
{
    adc_select_input(PROBE_VTARGET_ADC_INPUT);
    uint16_t raw = adc_read();
    // 12-bit reading against the 3.3V ADC reference, x2 to undo the divider.
    return ((uint32_t)raw * 3300 * 2) / 4095;
}
