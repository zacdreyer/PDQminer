/**
 * @file display_driver.c
 * @brief Display abstraction layer stub
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "display_driver.h"

#ifndef PDQ_HEADLESS

static PdqDisplayMode_t s_Mode = PdqDisplayModeMinimal;

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode) {
    s_Mode = Mode;
    return PdqOk;
}

PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats) {
    if (p_Stats == NULL) return PdqErrorInvalidParam;
    return PdqOk;
}

PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2) {
    return PdqOk;
}

PdqError_t PdqDisplaySetBrightness(uint8_t Percent) {
    return PdqOk;
}

PdqError_t PdqDisplayOff(void) {
    return PdqOk;
}

#endif
