#include <hardware/adc.h>

#include "probe_config.h"
#include "vtarget.h"

void vtarget_init(void)
{
    adc_init();
    /* Disables the digital input and pulls, which would bias the divider */
    adc_gpio_init(PROBE_VTARGET_ADC_PIN);
}

uint32_t vtarget_read_mv(void)
{
    adc_select_input(PROBE_VTARGET_ADC_INPUT);
    uint16_t raw = adc_read();
    /* 12-bit against 3.3V, x2 for the 10k/10k divider */
    return ((uint32_t)raw * 3300 * 2) / 4095;
}
