/**
 * @file main.cpp
 * @brief PDQminer entry point
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include <Arduino.h>
#include "pdq_types.h"
#include "hal/board_hal.h"
#include "config/config_manager.h"
#include "network/wifi_manager.h"
#include "stratum/stratum_client.h"
#include "core/mining_task.h"
#include "api/device_api.h"

#ifndef PDQ_HEADLESS
#include "display/display_driver.h"
#endif

static PdqDeviceConfig_t s_Config;
static PdqMinerStats_t s_Stats;
static PdqStratumJob_t s_StratumJob;
static uint8_t s_Extranonce1[PDQ_STRATUM_MAX_EXTRANONCE_LEN];
static uint8_t s_Extranonce1Len = 0;
static uint32_t s_Extranonce2 = 0;

#define SETUP_TIMEOUT_MS 30000

void setup() {
    Serial.begin(115200);
    Serial.println("\n[PDQminer] Starting...");
    Serial.printf("[PDQminer] Version: %d.%d.%d\n",
                  PDQ_VERSION_MAJOR, PDQ_VERSION_MINOR, PDQ_VERSION_PATCH);

    PdqHalInit();
    Serial.printf("[PDQminer] CPU: %lu MHz, Chip ID: %08X\n",
                  PdqHalGetCpuFreqMhz(), PdqHalGetChipId());

    PdqConfigInit();

#ifndef PDQ_HEADLESS
    PdqDisplayInit(PdqDisplayModeMinimal);
    PdqDisplayShowMessage("PDQminer", "Initializing...");
#endif

    if (!PdqConfigIsValid()) {
        Serial.println("[PDQminer] No valid config, starting portal...");
#ifndef PDQ_HEADLESS
        PdqDisplayShowMessage("PDQminer", "Setup Mode");
#endif
        PdqWifiInit();
        PdqWifiStartPortal();
        return;
    }

    PdqConfigLoad(&s_Config);

    PdqWifiInit();
    if (PdqWifiConnect(s_Config.Wifi.Ssid, s_Config.Wifi.Password) != PdqOk) {
        Serial.println("[PDQminer] WiFi failed, starting portal...");
        PdqWifiStartPortal();
        return;
    }

    char Ip[16];
    PdqWifiGetIp(Ip, sizeof(Ip));
    Serial.printf("[PDQminer] WiFi connected, IP: %s\n", Ip);

#ifndef PDQ_HEADLESS
    PdqDisplayShowMessage("PDQminer", "Connecting pool...");
#endif

    PdqStratumInit();
    if (PdqStratumConnect(s_Config.PrimaryPool.Host, s_Config.PrimaryPool.Port) != PdqOk) {
        Serial.println("[PDQminer] Pool connection failed");
        return;
    }

    PdqStratumSubscribe();

    uint32_t StartTime = millis();
    while (PdqStratumGetState() != StratumStateSubscribed) {
        PdqStratumProcess();
        if (millis() - StartTime > SETUP_TIMEOUT_MS) {
            Serial.println("[PDQminer] Subscribe timeout");
            return;
        }
        delay(100);
    }

    PdqStratumGetExtranonce(s_Extranonce1, &s_Extranonce1Len);

    char Worker[128];
    snprintf(Worker, sizeof(Worker), "%s.%s", s_Config.WalletAddress, s_Config.WorkerName);
    Worker[127] = '\0';
    PdqStratumAuthorize(Worker, "x");

    StartTime = millis();
    while (PdqStratumGetState() != StratumStateAuthorized &&
           PdqStratumGetState() != StratumStateReady) {
        PdqStratumProcess();
        if (millis() - StartTime > SETUP_TIMEOUT_MS) {
            Serial.println("[PDQminer] Authorize timeout");
            return;
        }
        delay(100);
    }

    PdqMiningInit();
    PdqMiningStart();

    PdqApiInit();
    PdqApiStart();

    Serial.println("[PDQminer] Mining started!");
}

void loop() {
    if (PdqWifiIsPortalActive()) {
        PdqWifiProcess();
        delay(100);
        return;
    }

    PdqStratumProcess();
    PdqApiProcess();

    if (PdqStratumHasNewJob()) {
        PdqStratumGetJob(&s_StratumJob);

        PdqMiningJob_t Job;
        s_Extranonce2++;

        PdqStratumBuildMiningJob(&s_StratumJob,
                                  s_Extranonce1, s_Extranonce1Len,
                                  s_Extranonce2, PdqStratumGetExtranonce2Size(),
                                  PdqStratumGetDifficulty(),
                                  &Job);

        Job.NonceStart = 0;
        Job.NonceEnd = 0xFFFFFFFF;
        PdqMiningSetJob(&Job);
    }

    while (PdqMiningHasShare()) {
        PdqShareInfo_t Share;
        if (PdqMiningGetShare(&Share) == PdqOk) {
            PdqStratumSubmitShare(Share.JobId, Share.Extranonce2, Share.Nonce, Share.NTime);
            Serial.printf("[PDQminer] Share submitted: nonce=%08X\n", Share.Nonce);
        }
    }

    PdqMiningGetStats(&s_Stats);

#ifndef PDQ_HEADLESS
    static uint32_t s_LastDisplayUpdate = 0;
    if (millis() - s_LastDisplayUpdate > 5000) {
        PdqDisplayUpdate(&s_Stats);
        s_LastDisplayUpdate = millis();
    }
#endif

    static uint32_t s_LastSerialUpdate = 0;
    if (millis() - s_LastSerialUpdate > 10000) {
        Serial.printf("[PDQminer] Hashrate: %lu KH/s | Shares: %lu | Blocks: %lu\n",
                      s_Stats.HashRate / 1000, s_Stats.SharesAccepted, s_Stats.BlocksFound);
        s_LastSerialUpdate = millis();
    }

    PdqHalFeedWdt();
    delay(10);
}