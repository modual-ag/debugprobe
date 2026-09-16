#ifndef VTARGET_H
#define VTARGET_H

#include <stdint.h>

// Call once at boot, before the first vtarget_read_mv().
void vtarget_init(void);

// Target rail voltage in millivolts, as sensed through the 10k/10k divider
// on GPIO29 (see board_universalflasher_config.h).
uint32_t vtarget_read_mv(void);

#endif
