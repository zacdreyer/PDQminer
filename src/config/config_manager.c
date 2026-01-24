/**
 * @file config_manager.c
 * @brief NVS configuration storage stub
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "config_manager.h"
#include <string.h>

static bool s_Valid = false;
static PdqDeviceConfig_t s_Config;

PdqError_t PdqConfigInit(void) {
    memset(&s_Config, 0, sizeof(s_Config));
    s_Valid = false;
    return PdqOk;
}

PdqError_t PdqConfigLoad(PdqDeviceConfig_t* p_Config) {
    if (p_Config == NULL) return PdqErrorInvalidParam;
    memcpy(p_Config, &s_Config, sizeof(PdqDeviceConfig_t));
    return PdqOk;
}

PdqError_t PdqConfigSave(const PdqDeviceConfig_t* p_Config) {
    if (p_Config == NULL) return PdqErrorInvalidParam;
    memcpy(&s_Config, p_Config, sizeof(PdqDeviceConfig_t));
    s_Valid = true;
    return PdqOk;
}

PdqError_t PdqConfigReset(void) {
    memset(&s_Config, 0, sizeof(s_Config));
    s_Valid = false;
    return PdqOk;
}

bool PdqConfigIsValid(void) {
    return s_Valid;
}
