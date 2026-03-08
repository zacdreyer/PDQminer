/**
 * @file linux_wifi.c
 * @brief Linux stub for WiFi manager
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 *
 * On Linux, networking is handled by the host OS. This stub reports
 * a connected state so the main loop proceeds to stratum connection.
 */

#include "network/wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

static PdqWifiState_t s_State = WifiStateDisconnected;
static char s_Ip[16] = "127.0.0.1";

PdqError_t PdqWifiInit(void) {
    s_State = WifiStateDisconnected;

    /* Detect primary IP from network interfaces */
    struct ifaddrs* iflist = NULL;
    if (getifaddrs(&iflist) == 0) {
        for (struct ifaddrs* ifa = iflist; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &sin->sin_addr, s_Ip, sizeof(s_Ip));
            break;
        }
        freeifaddrs(iflist);
    }
    return PdqOk;
}

PdqError_t PdqWifiConnect(const char* p_Ssid, const char* p_Password) {
    (void)p_Ssid;
    (void)p_Password;
    s_State = WifiStateConnected;
    printf("[WiFi] Linux host networking active, IP: %s\n", s_Ip);
    return PdqOk;
}

PdqError_t PdqWifiDisconnect(void) {
    s_State = WifiStateDisconnected;
    return PdqOk;
}

PdqError_t PdqWifiStartAp(const char* p_Ssid, const char* p_Password) {
    (void)p_Ssid;
    (void)p_Password;
    return PdqOk;
}

PdqError_t PdqWifiStopAp(void) {
    return PdqOk;
}

PdqError_t PdqWifiStartPortal(void) {
    printf("[WiFi] Portal not supported on Linux. Use --config or CLI args.\n");
    return PdqOk;
}

PdqError_t PdqWifiStopPortal(void) {
    return PdqOk;
}

PdqError_t PdqWifiProcess(void) {
    return PdqOk;
}

PdqError_t PdqWifiScan(PdqWifiScanResult_t* p_Results, uint8_t* p_Count) {
    if (p_Count) *p_Count = 0;
    (void)p_Results;
    return PdqOk;
}

bool PdqWifiIsConnected(void) {
    return s_State == WifiStateConnected;
}

bool PdqWifiIsPortalActive(void) {
    return false;
}

PdqWifiState_t PdqWifiGetState(void) {
    return s_State;
}

PdqError_t PdqWifiGetIp(char* p_Buffer, size_t BufferLen) {
    if (!p_Buffer || BufferLen == 0) return PdqErrorInvalidParam;
    snprintf(p_Buffer, BufferLen, "%s", s_Ip);
    return PdqOk;
}

int8_t PdqWifiGetRssi(void) {
    return -1;
}
