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
#include "core/sha256_engine.h"
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

    /* Run HW SHA diagnostic to determine midstate caching capability */
    PdqSha256HwDiagnostic();

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
    Serial.println("[DBG] Config loaded"); Serial.flush();

    PdqWifiInit();
    Serial.println("[DBG] WiFi init done"); Serial.flush();
    Serial.printf("[DBG] SSID='%s' len=%d\n", s_Config.Wifi.Ssid, strlen(s_Config.Wifi.Ssid));
    Serial.flush();
    if (PdqWifiConnect(s_Config.Wifi.Ssid, s_Config.Wifi.Password) != PdqOk) {
        Serial.println("[PDQminer] WiFi failed, starting portal...");
        PdqWifiStartPortal();
        return;
    }

    char Ip[16];
    PdqWifiGetIp(Ip, sizeof(Ip));
    Serial.printf("[PDQminer] WiFi connected, IP: %s\n", Ip);
    Serial.flush();

#ifndef PDQ_HEADLESS
    PdqDisplayShowMessage("PDQminer", "Connecting pool...");
#endif

    Serial.printf("[DBG] Connecting to %s:%d\n", s_Config.PrimaryPool.Host, s_Config.PrimaryPool.Port);
    Serial.flush();

    PdqStratumInit();
    Serial.println("[DBG] Stratum init done"); Serial.flush();

    if (PdqStratumConnect(s_Config.PrimaryPool.Host, s_Config.PrimaryPool.Port) != PdqOk) {
        Serial.println("[PDQminer] Pool connection failed");
        return;
    }
    Serial.println("[DBG] Pool connected"); Serial.flush();

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
    const char* PoolPass = s_Config.PrimaryPool.Password[0] ? s_Config.PrimaryPool.Password : "x";
    Serial.printf("[DBG] Authorizing with worker: '%s', password: '%s'\n", Worker, PoolPass);
    Serial.flush();
    PdqStratumAuthorize(Worker, PoolPass);

    StartTime = millis();
    while (PdqStratumGetState() != StratumStateAuthorized &&
           PdqStratumGetState() != StratumStateReady) {
        PdqStratumProcess();
        if (millis() - StartTime > SETUP_TIMEOUT_MS) {
            Serial.println("[PDQminer] Authorize timeout");
            Serial.printf("[DBG] State: %d\n", PdqStratumGetState());
            return;
        }
        delay(100);
    }
    Serial.println("[DBG] Authorization OK");

    PdqMiningInit();
    Serial.println("[DBG] Mining init done");
    PdqMiningStart();
    Serial.println("[DBG] Mining start called");

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

    static uint32_t s_LastJobCheck = 0;
    if (millis() - s_LastJobCheck > 5000) {
        Serial.printf("[DBG] StratumState=%d, StratumReady=%d\n",
                      PdqStratumGetState(), PdqStratumIsReady());
        s_LastJobCheck = millis();
    }

    if (PdqStratumHasNewJob()) {
        Serial.println("[DBG] New job received!");
        PdqStratumGetJob(&s_StratumJob);

        if (s_StratumJob.CleanJobs) {
            Serial.println("[DBG] Clean jobs - clearing share queue");
            PdqMiningClearShares();
        }

        PdqMiningJob_t Job;
        s_Extranonce2++;

        uint32_t Difficulty = PdqStratumGetDifficulty();
        Serial.printf("[DBG] Using difficulty: %lu\n", Difficulty);

        PdqStratumBuildMiningJob(&s_StratumJob,
                                  s_Extranonce1, s_Extranonce1Len,
                                  s_Extranonce2, PdqStratumGetExtranonce2Size(),
                                  Difficulty,
                                  &Job);

        Job.NonceStart = 0;
        Job.NonceEnd = 0xFFFFFFFF;
        PdqMiningSetJob(&Job);
        Serial.println("[DBG] Job set for mining");
    }

    if (PdqStratumIsReady()) {
        int SharesThisLoop = 0;
        while (PdqMiningHasShare() && SharesThisLoop < 5) {
            PdqShareInfo_t Share;
            if (PdqMiningGetShare(&Share) == PdqOk) {
                PdqStratumSubmitShare(Share.JobId, Share.Extranonce2, Share.Nonce, Share.NTime);
                Serial.printf("[PDQminer] Share submitted: nonce=%08X\n", Share.Nonce);
            }
            SharesThisLoop++;
        }
    }

    PdqMiningGetStats(&s_Stats);

#ifndef PDQ_HEADLESS
    static uint32_t s_LastDisplayUpdate = 0;
    if (millis() - s_LastDisplayUpdate > 500) {
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