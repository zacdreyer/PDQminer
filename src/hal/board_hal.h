/**
 * @file board_hal.h
 * @brief Hardware abstraction layer for board-specific features
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_BOARD_HAL_H
#define PDQ_BOARD_HAL_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqHalInit(void);
uint32_t   PdqHalGetCpuFreqMhz(void);
float      PdqHalGetTemperature(void);
uint32_t   PdqHalGetFreeHeap(void);
uint32_t   PdqHalGetChipId(void);
void       PdqHalRestart(void);
void       PdqHalFeedWdt(void);

#ifdef __cplusplus
}
#endif

#endif
