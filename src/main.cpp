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
    if (PdqWifiConnect(&s_Config.Wifi) != PdqOk) {
        Serial.println("[PDQminer] WiFi failed, starting portal...");
        PdqWifiStartPortal();
        return;
    }

    Serial.println("[PDQminer] WiFi connected");
#ifndef PDQ_HEADLESS
    PdqDisplayShowMessage("PDQminer", "Connecting pool...");
#endif

    PdqStratumInit();
    if (PdqStratumConnect(s_Config.PrimaryPool.Host, s_Config.PrimaryPool.Port) != PdqOk) {
        Serial.println("[PDQminer] Pool connection failed");
        return;
    }

    PdqStratumSubscribe();
    PdqStratumAuthorize(s_Config.WorkerName, "x");

    PdqMiningInit();
    PdqMiningStart();

    PdqApiInit();
    PdqApiStart();

    Serial.println("[PDQminer] Mining started!");
}

void loop() {
    if (PdqWifiIsPortalActive()) {
        delay(100);
        return;
    }

    PdqStratumProcess();
    PdqApiProcess();

    if (PdqStratumHasNewJob()) {
        PdqMiningJob_t Job;
        PdqStratumGetJob(&Job);
        PdqMiningSetJob(&Job);
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