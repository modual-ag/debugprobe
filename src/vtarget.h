#ifndef VTARGET_H
#define VTARGET_H

#include <stdint.h>

void vtarget_init(void);

/* Target supply voltage on the SWD header, in millivolts */
uint32_t vtarget_read_mv(void);

#endif
