/**
 * @file linux_config.c
 * @brief Linux implementation of config manager using JSON file
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 *
 * Replaces ESP32 NVS with a simple JSON key-value file stored at
 * ~/.pdqminer/config.json or a path specified by PDQ_CONFIG_PATH env var.
 *
 * JSON format is a flat object: {"key": "string_value", ...}
 * uint16 values are stored as decimal strings.
 */

#include "config/config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define PDQ_LINUX_CONFIG_MAX_KEYS   32
#define PDQ_LINUX_CONFIG_KEY_LEN    32
#define PDQ_LINUX_CONFIG_VAL_LEN    256
#define PDQ_LINUX_CONFIG_MAGIC_KEY  "__pdq_valid__"

typedef struct {
    char Key[PDQ_LINUX_CONFIG_KEY_LEN];
    char Value[PDQ_LINUX_CONFIG_VAL_LEN];
} KvPair_t;

static KvPair_t s_Store[PDQ_LINUX_CONFIG_MAX_KEYS];
static int      s_Count = 0;
static bool     s_Initialized = false;
static bool     s_Valid = false;
static char     s_ConfigPath[512] = {0};

/* ---- Internal helpers ---- */

static void GetConfigPath(void) {
    const char* env = getenv("PDQ_CONFIG_PATH");
    if (env && env[0]) {
        snprintf(s_ConfigPath, sizeof(s_ConfigPath), "%s", env);
        return;
    }
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(s_ConfigPath, sizeof(s_ConfigPath), "%s/.pdqminer/config.json", home);
}

static void EnsureDir(void) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", s_ConfigPath);
    /* Truncate at last '/' */
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0700);
    }
}

/* Minimal JSON parser — handles flat {"key":"value",...} objects.
 * We deliberately avoid external dependencies. */
static void LoadFile(void) {
    FILE* f = fopen(s_ConfigPath, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0 || len > 65536) { fclose(f); return; }
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return; }
    size_t nr = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[nr] = '\0';

    s_Count = 0;
    const char* p = buf;

    /* Skip to first '{' */
    while (*p && *p != '{') p++;
    if (*p == '{') p++;

    while (*p && s_Count < PDQ_LINUX_CONFIG_MAX_KEYS) {
        /* Skip whitespace / commas */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
        if (*p == '}' || *p == '\0') break;

        /* Expect "key" */
        if (*p != '"') break;
        p++;
        const char* kStart = p;
        while (*p && *p != '"') p++;
        size_t kLen = (size_t)(p - kStart);
        if (*p == '"') p++;

        /* Skip : */
        while (*p == ' ' || *p == '\t' || *p == ':') p++;

        /* Expect "value" */
        if (*p != '"') break;
        p++;
        const char* vStart = p;
        while (*p && *p != '"') p++;
        size_t vLen = (size_t)(p - vStart);
        if (*p == '"') p++;

        if (kLen > 0 && kLen < PDQ_LINUX_CONFIG_KEY_LEN && vLen < PDQ_LINUX_CONFIG_VAL_LEN) {
            memcpy(s_Store[s_Count].Key, kStart, kLen);
            s_Store[s_Count].Key[kLen] = '\0';
            memcpy(s_Store[s_Count].Value, vStart, vLen);
            s_Store[s_Count].Value[vLen] = '\0';
            s_Count++;
        }
    }

    free(buf);
}

static void SaveFile(void) {
    EnsureDir();
    FILE* f = fopen(s_ConfigPath, "w");
    if (!f) return;

    fprintf(f, "{\n");
    for (int i = 0; i < s_Count; i++) {
        fprintf(f, "  \"%s\": \"%s\"%s\n",
                s_Store[i].Key, s_Store[i].Value,
                (i < s_Count - 1) ? "," : "");
    }
    fprintf(f, "}\n");
    fclose(f);
}

static const char* KvGet(const char* key) {
    for (int i = 0; i < s_Count; i++) {
        if (strcmp(s_Store[i].Key, key) == 0) return s_Store[i].Value;
    }
    return NULL;
}

static void KvSet(const char* key, const char* value) {
    for (int i = 0; i < s_Count; i++) {
        if (strcmp(s_Store[i].Key, key) == 0) {
            snprintf(s_Store[i].Value, PDQ_LINUX_CONFIG_VAL_LEN, "%s", value);
            return;
        }
    }
    if (s_Count < PDQ_LINUX_CONFIG_MAX_KEYS) {
        snprintf(s_Store[s_Count].Key, PDQ_LINUX_CONFIG_KEY_LEN, "%s", key);
        snprintf(s_Store[s_Count].Value, PDQ_LINUX_CONFIG_VAL_LEN, "%s", value);
        s_Count++;
    }
}

/* ---- Public API (matches config_manager.h) ---- */

PdqError_t PdqConfigInit(void) {
    GetConfigPath();
    LoadFile();
    s_Initialized = true;

    const char* magic = KvGet(PDQ_LINUX_CONFIG_MAGIC_KEY);
    s_Valid = (magic != NULL && strcmp(magic, "1346371907") == 0); /* PDQ_CONFIG_MAGIC decimal */
    return PdqOk;
}

PdqError_t PdqConfigLoad(PdqDeviceConfig_t* p_Config) {
    if (!p_Config) return PdqErrorInvalidParam;
    memset(p_Config, 0, sizeof(PdqDeviceConfig_t));
    if (!s_Initialized) return PdqErrorNvsRead;

    const char* v;
    if ((v = KvGet(PDQ_CONFIG_KEY_WIFI_SSID))) snprintf(p_Config->Wifi.Ssid, PDQ_MAX_SSID_LEN + 1, "%s", v);
    if ((v = KvGet(PDQ_CONFIG_KEY_WIFI_PASS))) snprintf(p_Config->Wifi.Password, PDQ_MAX_PASSWORD_LEN + 1, "%s", v);
    if ((v = KvGet(PDQ_CONFIG_KEY_POOL1_HOST))) snprintf(p_Config->PrimaryPool.Host, PDQ_MAX_HOST_LEN + 1, "%s", v);
    if ((v = KvGet(PDQ_CONFIG_KEY_POOL1_PORT))) p_Config->PrimaryPool.Port = (uint16_t)atoi(v);
    if ((v = KvGet(PDQ_CONFIG_KEY_POOL2_HOST))) snprintf(p_Config->BackupPool.Host, PDQ_MAX_HOST_LEN + 1, "%s", v);
    if ((v = KvGet(PDQ_CONFIG_KEY_POOL2_PORT))) p_Config->BackupPool.Port = (uint16_t)atoi(v);
    if ((v = KvGet(PDQ_CONFIG_KEY_WALLET))) snprintf(p_Config->WalletAddress, PDQ_MAX_WALLET_LEN + 1, "%s", v);
    if ((v = KvGet(PDQ_CONFIG_KEY_WORKER))) snprintf(p_Config->WorkerName, PDQ_MAX_WORKER_LEN + 1, "%s", v);
    if ((v = KvGet(PDQ_CONFIG_KEY_DISPLAY))) p_Config->DisplayMode = (uint8_t)atoi(v);

    return PdqOk;
}

PdqError_t PdqConfigSave(const PdqDeviceConfig_t* p_Config) {
    if (!p_Config) return PdqErrorInvalidParam;
    if (!s_Initialized) return PdqErrorNvsWrite;

    KvSet(PDQ_CONFIG_KEY_WIFI_SSID, p_Config->Wifi.Ssid);
    KvSet(PDQ_CONFIG_KEY_WIFI_PASS, p_Config->Wifi.Password);
    KvSet(PDQ_CONFIG_KEY_POOL1_HOST, p_Config->PrimaryPool.Host);
    char portBuf[8];
    snprintf(portBuf, sizeof(portBuf), "%u", p_Config->PrimaryPool.Port);
    KvSet(PDQ_CONFIG_KEY_POOL1_PORT, portBuf);
    KvSet(PDQ_CONFIG_KEY_POOL2_HOST, p_Config->BackupPool.Host);
    snprintf(portBuf, sizeof(portBuf), "%u", p_Config->BackupPool.Port);
    KvSet(PDQ_CONFIG_KEY_POOL2_PORT, portBuf);
    KvSet(PDQ_CONFIG_KEY_WALLET, p_Config->WalletAddress);
    KvSet(PDQ_CONFIG_KEY_WORKER, p_Config->WorkerName);
    char dispBuf[4];
    snprintf(dispBuf, sizeof(dispBuf), "%u", p_Config->DisplayMode);
    KvSet(PDQ_CONFIG_KEY_DISPLAY, dispBuf);
    KvSet(PDQ_LINUX_CONFIG_MAGIC_KEY, "1346371907");

    s_Valid = true;
    SaveFile();
    return PdqOk;
}

PdqError_t PdqConfigReset(void) {
    s_Count = 0;
    s_Valid = false;
    SaveFile();
    return PdqOk;
}

bool PdqConfigIsValid(void) {
    return s_Valid;
}

PdqError_t PdqConfigGetString(const char* p_Key, char* p_Value, size_t MaxLen) {
    if (!p_Key || !p_Value || MaxLen == 0) return PdqErrorInvalidParam;
    if (!s_Initialized) return PdqErrorNvsRead;
    const char* v = KvGet(p_Key);
    if (v) {
        snprintf(p_Value, MaxLen, "%s", v);
    } else {
        p_Value[0] = '\0';
        return PdqErrorNvsRead;
    }
    return PdqOk;
}

PdqError_t PdqConfigSetString(const char* p_Key, const char* p_Value) {
    if (!p_Key || !p_Value) return PdqErrorInvalidParam;
    if (!s_Initialized) return PdqErrorNvsWrite;
    KvSet(p_Key, p_Value);
    SaveFile();
    return PdqOk;
}

PdqError_t PdqConfigGetU16(const char* p_Key, uint16_t* p_Value) {
    if (!p_Key || !p_Value) return PdqErrorInvalidParam;
    if (!s_Initialized) return PdqErrorNvsRead;
    const char* v = KvGet(p_Key);
    if (v) {
        *p_Value = (uint16_t)atoi(v);
    } else {
        *p_Value = 0;
        return PdqErrorNvsRead;
    }
    return PdqOk;
}

PdqError_t PdqConfigSetU16(const char* p_Key, uint16_t Value) {
    if (!p_Key) return PdqErrorInvalidParam;
    if (!s_Initialized) return PdqErrorNvsWrite;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", Value);
    KvSet(p_Key, buf);
    SaveFile();
    return PdqOk;
}
