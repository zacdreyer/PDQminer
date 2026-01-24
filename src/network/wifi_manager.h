/**
 * @file wifi_manager.h
 * @brief WiFi connection and captive portal management
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_WIFI_MANAGER_H
#define PDQ_WIFI_MANAGER_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqWifiInit(void);
PdqError_t PdqWifiConnect(const PdqWifiConfig_t* p_Config);
PdqError_t PdqWifiDisconnect(void);
PdqError_t PdqWifiStartPortal(void);
PdqError_t PdqWifiStopPortal(void);
bool       PdqWifiIsConnected(void);
bool       PdqWifiIsPortalActive(void);
PdqError_t PdqWifiGetIp(char* p_Buffer, size_t BufferLen);

#ifdef __cplusplus
}
#endif

#endif
