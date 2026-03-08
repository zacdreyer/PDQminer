/**
 * @file display_driver.h
 * @brief Display abstraction layer for ILI9341/ST7789
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_DISPLAY_DRIVER_H
#define PDQ_DISPLAY_DRIVER_H

#include "../pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PdqDisplayModeHeadless = 0,
    PdqDisplayModeMinimal,
    PdqDisplayModeStandard,
    PdqDisplayModeTrinityPro,
    PdqDisplayModeTrinityLite
} PdqDisplayMode_t;

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode);
PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats);
PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2);
PdqError_t PdqDisplayShowInfo(const char* p_Pool, const char* p_Ip, const char* p_Worker);
PdqError_t PdqDisplaySetBrightness(uint8_t Percent);
PdqError_t PdqDisplayOff(void);
PdqDisplayMode_t PdqDisplayCycleTheme(void);

#ifdef __cplusplus
}
#endif

#endif
