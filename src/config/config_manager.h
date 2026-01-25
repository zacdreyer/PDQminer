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

#define PDQ_CONFIG_NAMESPACE      "pdqminer"
#define PDQ_CONFIG_KEY_WIFI_SSID  "wifi_ssid"
#define PDQ_CONFIG_KEY_WIFI_PASS  "wifi_pass"
#define PDQ_CONFIG_KEY_POOL1_HOST "pool1_host"
#define PDQ_CONFIG_KEY_POOL1_PORT "pool1_port"
#define PDQ_CONFIG_KEY_POOL2_HOST "pool2_host"
#define PDQ_CONFIG_KEY_POOL2_PORT "pool2_port"
#define PDQ_CONFIG_KEY_WALLET     "wallet"
#define PDQ_CONFIG_KEY_WORKER     "worker"
#define PDQ_CONFIG_KEY_DISPLAY    "display"
#define PDQ_CONFIG_KEY_VALID      "valid"

#define PDQ_CONFIG_MAGIC          0x50445143

PdqError_t PdqConfigInit(void);
PdqError_t PdqConfigLoad(PdqDeviceConfig_t* p_Config);
PdqError_t PdqConfigSave(const PdqDeviceConfig_t* p_Config);
PdqError_t PdqConfigReset(void);
bool       PdqConfigIsValid(void);
PdqError_t PdqConfigGetString(const char* p_Key, char* p_Value, size_t MaxLen);
PdqError_t PdqConfigSetString(const char* p_Key, const char* p_Value);
PdqError_t PdqConfigGetU16(const char* p_Key, uint16_t* p_Value);
PdqError_t PdqConfigSetU16(const char* p_Key, uint16_t Value);

#ifdef __cplusplus
}
#endif

#endif
