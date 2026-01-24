/**
 * @file wifi_manager.c
 * @brief WiFi connection and captive portal stub
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "wifi_manager.h"
#include <string.h>

static bool s_Connected = false;
static bool s_PortalActive = false;

PdqError_t PdqWifiInit(void) {
    s_Connected = false;
    s_PortalActive = false;
    return PdqOk;
}

PdqError_t PdqWifiConnect(const PdqWifiConfig_t* p_Config) {
    if (p_Config == NULL) return PdqErrorInvalidParam;
    s_Connected = true;
    return PdqOk;
}

PdqError_t PdqWifiDisconnect(void) {
    s_Connected = false;
    return PdqOk;
}

PdqError_t PdqWifiStartPortal(void) {
    s_PortalActive = true;
    return PdqOk;
}

PdqError_t PdqWifiStopPortal(void) {
    s_PortalActive = false;
    return PdqOk;
}

bool PdqWifiIsConnected(void) {
    return s_Connected;
}

bool PdqWifiIsPortalActive(void) {
    return s_PortalActive;
}

PdqError_t PdqWifiGetIp(char* p_Buffer, size_t BufferLen) {
    if (p_Buffer == NULL || BufferLen < 16) return PdqErrorBufferTooSmall;
    strncpy(p_Buffer, "0.0.0.0", BufferLen);
    return PdqOk;
}
