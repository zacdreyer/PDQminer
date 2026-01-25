/**
 * @file config_manager.c
 * @brief NVS configuration storage implementation
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "config_manager.h"
#include <string.h>

#ifdef ESP32
#include <nvs_flash.h>
#include <nvs.h>

static nvs_handle_t s_NvsHandle = 0;
static bool s_Initialized = false;
#endif

static bool s_Valid = false;

PdqError_t PdqConfigInit(void)
{
#ifdef ESP32
    esp_err_t Err = nvs_flash_init();
    if (Err == ESP_ERR_NVS_NO_FREE_PAGES || Err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        Err = nvs_flash_init();
    }
    if (Err != ESP_OK) return PdqErrorNvsRead;
    
    Err = nvs_open(PDQ_CONFIG_NAMESPACE, NVS_READWRITE, &s_NvsHandle);
    if (Err != ESP_OK) return PdqErrorNvsRead;
    
    s_Initialized = true;
    
    uint32_t Magic = 0;
    Err = nvs_get_u32(s_NvsHandle, PDQ_CONFIG_KEY_VALID, &Magic);
    s_Valid = (Err == ESP_OK && Magic == PDQ_CONFIG_MAGIC);
#endif
    
    return PdqOk;
}

PdqError_t PdqConfigLoad(PdqDeviceConfig_t* p_Config)
{
    if (p_Config == NULL) return PdqErrorInvalidParam;
    
    memset(p_Config, 0, sizeof(PdqDeviceConfig_t));
    
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsRead;
    
    size_t Len;
    
    Len = PDQ_MAX_SSID_LEN + 1;
    nvs_get_str(s_NvsHandle, PDQ_CONFIG_KEY_WIFI_SSID, p_Config->Wifi.Ssid, &Len);
    
    Len = PDQ_MAX_PASSWORD_LEN + 1;
    nvs_get_str(s_NvsHandle, PDQ_CONFIG_KEY_WIFI_PASS, p_Config->Wifi.Password, &Len);
    
    Len = PDQ_MAX_HOST_LEN + 1;
    nvs_get_str(s_NvsHandle, PDQ_CONFIG_KEY_POOL1_HOST, p_Config->PrimaryPool.Host, &Len);
    nvs_get_u16(s_NvsHandle, PDQ_CONFIG_KEY_POOL1_PORT, &p_Config->PrimaryPool.Port);
    
    Len = PDQ_MAX_HOST_LEN + 1;
    nvs_get_str(s_NvsHandle, PDQ_CONFIG_KEY_POOL2_HOST, p_Config->BackupPool.Host, &Len);
    nvs_get_u16(s_NvsHandle, PDQ_CONFIG_KEY_POOL2_PORT, &p_Config->BackupPool.Port);
    
    Len = PDQ_MAX_WALLET_LEN + 1;
    nvs_get_str(s_NvsHandle, PDQ_CONFIG_KEY_WALLET, p_Config->WalletAddress, &Len);
    
    Len = PDQ_MAX_WORKER_LEN + 1;
    nvs_get_str(s_NvsHandle, PDQ_CONFIG_KEY_WORKER, p_Config->WorkerName, &Len);
    
    nvs_get_u8(s_NvsHandle, PDQ_CONFIG_KEY_DISPLAY, &p_Config->DisplayMode);
#endif
    
    return PdqOk;
}

PdqError_t PdqConfigSave(const PdqDeviceConfig_t* p_Config)
{
    if (p_Config == NULL) return PdqErrorInvalidParam;
    
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsWrite;
    
    nvs_set_str(s_NvsHandle, PDQ_CONFIG_KEY_WIFI_SSID, p_Config->Wifi.Ssid);
    nvs_set_str(s_NvsHandle, PDQ_CONFIG_KEY_WIFI_PASS, p_Config->Wifi.Password);
    nvs_set_str(s_NvsHandle, PDQ_CONFIG_KEY_POOL1_HOST, p_Config->PrimaryPool.Host);
    nvs_set_u16(s_NvsHandle, PDQ_CONFIG_KEY_POOL1_PORT, p_Config->PrimaryPool.Port);
    nvs_set_str(s_NvsHandle, PDQ_CONFIG_KEY_POOL2_HOST, p_Config->BackupPool.Host);
    nvs_set_u16(s_NvsHandle, PDQ_CONFIG_KEY_POOL2_PORT, p_Config->BackupPool.Port);
    nvs_set_str(s_NvsHandle, PDQ_CONFIG_KEY_WALLET, p_Config->WalletAddress);
    nvs_set_str(s_NvsHandle, PDQ_CONFIG_KEY_WORKER, p_Config->WorkerName);
    nvs_set_u8(s_NvsHandle, PDQ_CONFIG_KEY_DISPLAY, p_Config->DisplayMode);
    nvs_set_u32(s_NvsHandle, PDQ_CONFIG_KEY_VALID, PDQ_CONFIG_MAGIC);
    
    esp_err_t Err = nvs_commit(s_NvsHandle);
    if (Err != ESP_OK) return PdqErrorNvsWrite;
    
    s_Valid = true;
#endif
    
    return PdqOk;
}

PdqError_t PdqConfigReset(void)
{
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsWrite;
    
    nvs_erase_all(s_NvsHandle);
    nvs_commit(s_NvsHandle);
    s_Valid = false;
#endif
    
    return PdqOk;
}

bool PdqConfigIsValid(void)
{
    return s_Valid;
}

PdqError_t PdqConfigGetString(const char* p_Key, char* p_Value, size_t MaxLen)
{
    if (p_Key == NULL || p_Value == NULL || MaxLen == 0) return PdqErrorInvalidParam;
    
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsRead;
    
    size_t Len = MaxLen;
    esp_err_t Err = nvs_get_str(s_NvsHandle, p_Key, p_Value, &Len);
    if (Err != ESP_OK) {
        p_Value[0] = '\0';
        return PdqErrorNvsRead;
    }
#else
    p_Value[0] = '\0';
#endif
    
    return PdqOk;
}

PdqError_t PdqConfigSetString(const char* p_Key, const char* p_Value)
{
    if (p_Key == NULL || p_Value == NULL) return PdqErrorInvalidParam;
    
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsWrite;
    
    esp_err_t Err = nvs_set_str(s_NvsHandle, p_Key, p_Value);
    if (Err != ESP_OK) return PdqErrorNvsWrite;
    
    nvs_commit(s_NvsHandle);
#endif
    
    return PdqOk;
}

PdqError_t PdqConfigGetU16(const char* p_Key, uint16_t* p_Value)
{
    if (p_Key == NULL || p_Value == NULL) return PdqErrorInvalidParam;
    
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsRead;
    
    esp_err_t Err = nvs_get_u16(s_NvsHandle, p_Key, p_Value);
    if (Err != ESP_OK) {
        *p_Value = 0;
        return PdqErrorNvsRead;
    }
#else
    *p_Value = 0;
#endif
    
    return PdqOk;
}

PdqError_t PdqConfigSetU16(const char* p_Key, uint16_t Value)
{
    if (p_Key == NULL) return PdqErrorInvalidParam;
    
#ifdef ESP32
    if (!s_Initialized) return PdqErrorNvsWrite;
    
    esp_err_t Err = nvs_set_u16(s_NvsHandle, p_Key, Value);
    if (Err != ESP_OK) return PdqErrorNvsWrite;
    
    nvs_commit(s_NvsHandle);
#endif
    
    return PdqOk;
}
