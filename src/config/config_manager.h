/**
 * @file config_manager.h
 * @brief NVS configuration storage
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_CONFIG_MANAGER_H
#define PDQ_CONFIG_MANAGER_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqConfigInit(void);
PdqError_t PdqConfigLoad(PdqDeviceConfig_t* p_Config);
PdqError_t PdqConfigSave(const PdqDeviceConfig_t* p_Config);
PdqError_t PdqConfigReset(void);
bool       PdqConfigIsValid(void);

#ifdef __cplusplus
}
#endif

#endif
