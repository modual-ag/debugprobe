/*
 * Copyright (c) 2026 modual AG
 *
 * SPDX-License-Identifier: MIT
 */

// Pico SDK board definition for the UniversalFlasher (RP2040). Selected by PICO_BOARD=universalflasher,
// found through PICO_BOARD_HEADER_DIRS. Clock and flash settings are the Raspberry Pi Pico's, which the
// board has always been built with; the debugprobe pin map is in include/board_universalflasher_config.h.

#ifndef _BOARDS_UNIVERSALFLASHER_H
#define _BOARDS_UNIVERSALFLASHER_H

pico_board_cmake_set(PICO_PLATFORM, rp2040)

// For board detection (include/board_config.h)
#define UNIVERSALFLASHER

// No default UART: GPIO0/1 are the ESP32 UART on this board

// --- FLASH ---

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (2 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 1
#endif

#endif
