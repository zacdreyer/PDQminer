/**
 * @file wifi_manager.c
 * @brief WiFi connection and captive portal implementation
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "wifi_manager.h"
#include "config/config_manager.h"
#include <string.h>
#include <stdio.h>

#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>

static WebServer* s_pServer = NULL;
static DNSServer* s_pDns = NULL;
#endif

typedef struct {
    PdqWifiState_t State;
    char           Ssid[PDQ_MAX_SSID_LEN + 1];
    char           Password[PDQ_MAX_PASSWORD_LEN + 1];
    char           ApSsid[PDQ_MAX_SSID_LEN + 1];
    uint32_t       ConnectStartTime;
    uint32_t       PortalStartTime;
    bool           PortalActive;
} WifiContext_t;

static WifiContext_t s_Ctx;

#ifdef ESP32
static const char* s_HtmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PDQminer Setup</title>
<style>
body{font-family:sans-serif;max-width:400px;margin:0 auto;padding:20px}
input,select{width:100%;padding:8px;margin:5px 0 15px;box-sizing:border-box}
button{background:#007bff;color:#fff;padding:10px 20px;border:none;cursor:pointer;width:100%}
.section{border:1px solid #ddd;padding:15px;margin:10px 0;border-radius:5px}
h3{margin-top:0}
</style>
</head>
<body>
<h1>PDQminer Setup</h1>
<form action="/save" method="POST">
<div class="section">
<h3>WiFi Network</h3>
<label>SSID</label><input type="text" name="ssid" id="ssid" maxlength="32" required>
<label>Password</label><input type="password" name="wifi_pass" maxlength="64">
</div>
<div class="section">
<h3>Primary Pool</h3>
<label>Host</label><input type="text" name="pool1_host" value="public-pool.io" maxlength="63">
<label>Port</label><input type="number" name="pool1_port" value="21496" min="1" max="65535">
</div>
<div class="section">
<h3>Wallet</h3>
<label>BTC Address</label><input type="text" name="wallet" maxlength="62" required>
<label>Worker Name</label><input type="text" name="worker" value="pdqminer" maxlength="31">
</div>
<button type="submit">Save & Reboot</button>
</form>
</body>
</html>
)rawliteral";

static void HandleRoot(void)
{
    if (s_pServer) {
        s_pServer->send(200, "text/html", s_HtmlPage);
    }
}

static void HandleSave(void)
{
    if (!s_pServer) return;

    PdqDeviceConfig_t Config;
    memset(&Config, 0, sizeof(Config));

    if (s_pServer->hasArg("ssid")) {
        strncpy(Config.Wifi.Ssid, s_pServer->arg("ssid").c_str(), PDQ_MAX_SSID_LEN);
        Config.Wifi.Ssid[PDQ_MAX_SSID_LEN] = '\0';
    }
    if (s_pServer->hasArg("wifi_pass")) {
        strncpy(Config.Wifi.Password, s_pServer->arg("wifi_pass").c_str(), PDQ_MAX_PASSWORD_LEN);
        Config.Wifi.Password[PDQ_MAX_PASSWORD_LEN] = '\0';
    }
    if (s_pServer->hasArg("pool1_host")) {
        strncpy(Config.PrimaryPool.Host, s_pServer->arg("pool1_host").c_str(), PDQ_MAX_HOST_LEN);
        Config.PrimaryPool.Host[PDQ_MAX_HOST_LEN] = '\0';
    }
    if (s_pServer->hasArg("pool1_port")) {
        Config.PrimaryPool.Port = (uint16_t)s_pServer->arg("pool1_port").toInt();
    }
    if (s_pServer->hasArg("wallet")) {
        strncpy(Config.WalletAddress, s_pServer->arg("wallet").c_str(), PDQ_MAX_WALLET_LEN);
        Config.WalletAddress[PDQ_MAX_WALLET_LEN] = '\0';
    }
    if (s_pServer->hasArg("worker")) {
        strncpy(Config.WorkerName, s_pServer->arg("worker").c_str(), PDQ_MAX_WORKER_LEN);
        Config.WorkerName[PDQ_MAX_WORKER_LEN] = '\0';
    }

    PdqConfigSave(&Config);

    s_pServer->send(200, "text/html", "<h1>Saved! Rebooting...</h1>");
    delay(1000);
    ESP.restart();
}

static void HandleNotFound(void)
{
    if (s_pServer) {
        s_pServer->sendHeader("Location", "/", true);
        s_pServer->send(302, "text/plain", "");
    }
}
#endif

PdqError_t PdqWifiInit(void)
{
    memset(&s_Ctx, 0, sizeof(s_Ctx));
    s_Ctx.State = WifiStateDisconnected;
    
#ifdef ESP32
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    uint8_t Mac[6];
    esp_read_mac(Mac, ESP_MAC_WIFI_STA);
    snprintf(s_Ctx.ApSsid, sizeof(s_Ctx.ApSsid), "%s%02X%02X", 
             PDQ_WIFI_AP_SSID_PREFIX, Mac[4], Mac[5]);
#endif
    
    return PdqOk;
}

PdqError_t PdqWifiConnect(const char* p_Ssid, const char* p_Password)
{
    if (p_Ssid == NULL || strlen(p_Ssid) == 0) return PdqErrorInvalidParam;

    strncpy(s_Ctx.Ssid, p_Ssid, PDQ_MAX_SSID_LEN);
    s_Ctx.Ssid[PDQ_MAX_SSID_LEN] = '\0';
    if (p_Password) {
        strncpy(s_Ctx.Password, p_Password, PDQ_MAX_PASSWORD_LEN);
        s_Ctx.Password[PDQ_MAX_PASSWORD_LEN] = '\0';
    }

#ifdef ESP32
    WiFi.mode(WIFI_STA);
    WiFi.begin(s_Ctx.Ssid, s_Ctx.Password);
    s_Ctx.State = WifiStateConnecting;
    s_Ctx.ConnectStartTime = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - s_Ctx.ConnectStartTime > PDQ_WIFI_CONNECT_TIMEOUT) {
            s_Ctx.State = WifiStateDisconnected;
            return PdqErrorTimeout;
        }
        delay(100);
    }
    
    s_Ctx.State = WifiStateConnected;
#endif
    
    return PdqOk;
}

PdqError_t PdqWifiDisconnect(void)
{
#ifdef ESP32
    WiFi.disconnect();
#endif
    s_Ctx.State = WifiStateDisconnected;
    return PdqOk;
}

PdqError_t PdqWifiStartAp(const char* p_Ssid, const char* p_Password)
{
    const char* Ssid = (p_Ssid && strlen(p_Ssid) > 0) ? p_Ssid : s_Ctx.ApSsid;
    
#ifdef ESP32
    WiFi.mode(WIFI_AP);
    if (p_Password && strlen(p_Password) >= 8) {
        WiFi.softAP(Ssid, p_Password);
    } else {
        WiFi.softAP(Ssid);
    }
    
    IPAddress Local(192, 168, 4, 1);
    IPAddress Gateway(192, 168, 4, 1);
    IPAddress Subnet(255, 255, 255, 0);
    WiFi.softAPConfig(Local, Gateway, Subnet);
#endif
    
    s_Ctx.State = WifiStateApMode;
    return PdqOk;
}

PdqError_t PdqWifiStopAp(void)
{
#ifdef ESP32
    WiFi.softAPdisconnect(true);
#endif
    s_Ctx.State = WifiStateDisconnected;
    return PdqOk;
}

PdqError_t PdqWifiStartPortal(void)
{
    PdqError_t Err = PdqWifiStartAp(NULL, NULL);
    if (Err != PdqOk) return Err;
    
#ifdef ESP32
    s_pDns = new DNSServer();
    s_pServer = new WebServer(80);
    
    s_pDns->start(53, "*", WiFi.softAPIP());
    
    s_pServer->on("/", HandleRoot);
    s_pServer->on("/save", HTTP_POST, HandleSave);
    s_pServer->onNotFound(HandleNotFound);
    s_pServer->begin();
#endif
    
    s_Ctx.PortalActive = true;
    s_Ctx.PortalStartTime = 0;
#ifdef ESP32
    s_Ctx.PortalStartTime = millis();
#endif
    s_Ctx.State = WifiStatePortalActive;
    
    return PdqOk;
}

PdqError_t PdqWifiStopPortal(void)
{
#ifdef ESP32
    if (s_pServer) {
        s_pServer->stop();
        delete s_pServer;
        s_pServer = NULL;
    }
    if (s_pDns) {
        s_pDns->stop();
        delete s_pDns;
        s_pDns = NULL;
    }
#endif
    
    s_Ctx.PortalActive = false;
    return PdqWifiStopAp();
}

PdqError_t PdqWifiProcess(void)
{
#ifdef ESP32
    if (s_Ctx.PortalActive) {
        if (s_pDns) s_pDns->processNextRequest();
        if (s_pServer) s_pServer->handleClient();
        
        if (s_Ctx.PortalStartTime > 0 && 
            (millis() - s_Ctx.PortalStartTime) > PDQ_WIFI_PORTAL_TIMEOUT) {
            PdqWifiStopPortal();
            return PdqErrorTimeout;
        }
    }
    
    if (s_Ctx.State == WifiStateConnected && WiFi.status() != WL_CONNECTED) {
        s_Ctx.State = WifiStateDisconnected;
    }
#endif
    
    return PdqOk;
}

PdqError_t PdqWifiScan(PdqWifiScanResult_t* p_Results, uint8_t* p_Count)
{
    if (p_Results == NULL || p_Count == NULL) return PdqErrorInvalidParam;
    
    *p_Count = 0;
    
#ifdef ESP32
    int16_t n = WiFi.scanNetworks();
    if (n < 0) return PdqErrorTimeout;
    
    uint8_t Count = (n > PDQ_WIFI_MAX_SCAN_RESULTS) ? PDQ_WIFI_MAX_SCAN_RESULTS : (uint8_t)n;
    
    for (uint8_t i = 0; i < Count; i++) {
        strncpy(p_Results[i].Ssid, WiFi.SSID(i).c_str(), PDQ_MAX_SSID_LEN);
        p_Results[i].Ssid[PDQ_MAX_SSID_LEN] = '\0';
        p_Results[i].Rssi = (int8_t)WiFi.RSSI(i);
        p_Results[i].Channel = (uint8_t)WiFi.channel(i);
        p_Results[i].Secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    *p_Count = Count;
    WiFi.scanDelete();
#endif
    
    return PdqOk;
}

bool PdqWifiIsConnected(void)
{
#ifdef ESP32
    return (s_Ctx.State == WifiStateConnected && WiFi.status() == WL_CONNECTED);
#else
    return (s_Ctx.State == WifiStateConnected);
#endif
}

bool PdqWifiIsPortalActive(void)
{
    return s_Ctx.PortalActive;
}

PdqWifiState_t PdqWifiGetState(void)
{
    return s_Ctx.State;
}

PdqError_t PdqWifiGetIp(char* p_Buffer, size_t BufferLen)
{
    if (p_Buffer == NULL || BufferLen < 16) return PdqErrorInvalidParam;
    
#ifdef ESP32
    if (s_Ctx.State == WifiStateConnected) {
        IPAddress Ip = WiFi.localIP();
        snprintf(p_Buffer, BufferLen, "%d.%d.%d.%d", Ip[0], Ip[1], Ip[2], Ip[3]);
    } else if (s_Ctx.State == WifiStateApMode || s_Ctx.State == WifiStatePortalActive) {
        IPAddress Ip = WiFi.softAPIP();
        snprintf(p_Buffer, BufferLen, "%d.%d.%d.%d", Ip[0], Ip[1], Ip[2], Ip[3]);
    } else {
        strncpy(p_Buffer, "0.0.0.0", BufferLen);
    }
#else
    strncpy(p_Buffer, "0.0.0.0", BufferLen);
#endif
    
    return PdqOk;
}

int8_t PdqWifiGetRssi(void)
{
#ifdef ESP32
    if (s_Ctx.State == WifiStateConnected) {
        return (int8_t)WiFi.RSSI();
    }
#endif
    return 0;
}
