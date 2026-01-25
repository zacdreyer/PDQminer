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

#define PDQ_WIFI_AP_SSID_PREFIX    "PDQminer_"
#define PDQ_WIFI_AP_IP             "192.168.4.1"
#define PDQ_WIFI_AP_GATEWAY        "192.168.4.1"
#define PDQ_WIFI_AP_SUBNET         "255.255.255.0"
#define PDQ_WIFI_CONNECT_TIMEOUT   30000
#define PDQ_WIFI_PORTAL_TIMEOUT    300000
#define PDQ_WIFI_MAX_SCAN_RESULTS  20

typedef enum {
    WifiStateDisconnected = 0,
    WifiStateConnecting,
    WifiStateConnected,
    WifiStateApMode,
    WifiStatePortalActive
} PdqWifiState_t;

typedef struct {
    char     Ssid[PDQ_MAX_SSID_LEN + 1];
    int8_t   Rssi;
    uint8_t  Channel;
    bool     Secure;
} PdqWifiScanResult_t;

PdqError_t      PdqWifiInit(void);
PdqError_t      PdqWifiConnect(const char* p_Ssid, const char* p_Password);
PdqError_t      PdqWifiDisconnect(void);
PdqError_t      PdqWifiStartAp(const char* p_Ssid, const char* p_Password);
PdqError_t      PdqWifiStopAp(void);
PdqError_t      PdqWifiStartPortal(void);
PdqError_t      PdqWifiStopPortal(void);
PdqError_t      PdqWifiProcess(void);
PdqError_t      PdqWifiScan(PdqWifiScanResult_t* p_Results, uint8_t* p_Count);

bool            PdqWifiIsConnected(void);
bool            PdqWifiIsPortalActive(void);
PdqWifiState_t  PdqWifiGetState(void);
PdqError_t      PdqWifiGetIp(char* p_Buffer, size_t BufferLen);
int8_t          PdqWifiGetRssi(void);

#ifdef __cplusplus
}
#endif

#endif
