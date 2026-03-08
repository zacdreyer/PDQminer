/**
 * @file linux_display.c
 * @brief Linux headless display stub
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "display/display_driver.h"

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode) {
    (void)Mode;
    return PdqOk;
}

PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats) {
    (void)p_Stats;
    return PdqOk;
}

PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2) {
    (void)p_Line1;
    (void)p_Line2;
    return PdqOk;
}

PdqError_t PdqDisplaySetBrightness(uint8_t Percent) {
    (void)Percent;
    return PdqOk;
}

PdqError_t PdqDisplayOff(void) {
    return PdqOk;
}
