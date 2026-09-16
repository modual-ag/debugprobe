#include "probe_config.h"
#include "pico/binary_info.h"
#include "probe/version.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


void bi_decl_config()
{
    bi_decl(bi_program_version_string(PROBE_VERSION));
#ifdef PROBE_PIN_RESET
    bi_decl(bi_1pin_with_name(PROBE_PIN_RESET, "PROBE RESET"));
#endif

#ifdef PROBE_PIN_SWCLK
    bi_decl(bi_1pin_with_name(PROBE_PIN_SWCLK, "PROBE SWCLK"));
#endif

#ifdef PROBE_PIN_SWDIO
    bi_decl(bi_1pin_with_name(PROBE_PIN_SWDIO, "PROBE SWDIO"));
#endif

#ifdef PROBE_PIN_SWDI
    bi_decl(bi_1pin_with_name(PROBE_PIN_SWDI, "PROBE SWDI"));
#endif

#ifdef PROBE_PIN_SWDIOEN
    bi_decl(bi_1pin_with_name(PROBE_PIN_SWDIOEN, "PROBE SWDIOEN"));
#endif

#ifdef PROBE_CDC_UART
    bi_decl(bi_program_feature("PROBE UART0 (ESP32) INTERFACE " STR(PROBE_UART0_INTERFACE)));
    bi_decl(bi_program_feature("PROBE UART0 (ESP32) BAUDRATE " STR(PROBE_UART0_BAUDRATE)));
    bi_decl(bi_1pin_with_name(PROBE_UART0_TX, "PROBE UART0 TX"));
    bi_decl(bi_1pin_with_name(PROBE_UART0_RX, "PROBE UART0 RX"));
    bi_decl(bi_1pin_with_name(PROBE_UART0_DTR, "PROBE UART0 nDTR (ESP32 EN)"));
    bi_decl(bi_1pin_with_name(PROBE_UART0_RTS, "PROBE UART0 nRTS (ESP32 IO0)"));

    bi_decl(bi_program_feature("PROBE UART1 (LOG) INTERFACE " STR(PROBE_UART1_INTERFACE)));
    bi_decl(bi_program_feature("PROBE UART1 (LOG) BAUDRATE " STR(PROBE_UART1_BAUDRATE)));
    bi_decl(bi_1pin_with_name(PROBE_UART1_TX, "PROBE UART1 TX"));
    bi_decl(bi_1pin_with_name(PROBE_UART1_RX, "PROBE UART1 RX"));
#ifdef PROBE_UART1_HWFC
    bi_decl(bi_1pin_with_name(PROBE_UART1_CTS, "PROBE UART1 CTS"));
    bi_decl(bi_1pin_with_name(PROBE_UART1_RTS, "PROBE UART1 RTS"));
#endif
#endif

}
