/**
 * @file main_benchmark.cpp
 * @brief Standalone benchmark firmware for measuring raw hashrate
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 * 
 * Flash this to measure baseline hashrate without WiFi/Stratum overhead.
 * Build with: pio run -e esp32_headless -t upload
 */

#include <Arduino.h>
#include "esp_task_wdt.h"
#include "pdq_types.h"
#include "core/sha256_engine.h"
#include "core/mining_task.h"
#include "hal/board_hal.h"

static const uint8_t TEST_BLOCK[80] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3b, 0xa3, 0xed, 0xfd,
    0x7a, 0x7b, 0x12, 0xb2, 0x7a, 0xc7, 0x2c, 0x3e,
    0x67, 0x76, 0x8f, 0x61, 0x7f, 0xc8, 0x1b, 0xc3,
    0x88, 0x8a, 0x51, 0x32, 0x3a, 0x9f, 0xb8, 0xaa,
    0x4b, 0x1e, 0x5e, 0x4a, 0x29, 0xab, 0x5f, 0x49,
    0xff, 0xff, 0x00, 0x1d, 0x1d, 0xac, 0x2b, 0x7c
};

static volatile uint64_t g_TotalHashes = 0;
static volatile uint32_t g_HashRate = 0;

void BenchmarkTask(void* p_Param) {
    int CoreId = xPortGetCoreID();
    uint32_t LocalHashes = 0;
    uint32_t LastReport = millis();

    PdqMiningJob_t Job;
    PdqSha256Midstate(TEST_BLOCK, Job.Midstate);
    memcpy(Job.BlockTail, TEST_BLOCK + 64, 16);
    memset(Job.BlockTail + 16, 0x80, 1);
    memset(Job.BlockTail + 17, 0, 39);
    Job.BlockTail[62] = 0x02;
    Job.BlockTail[63] = 0x80;
    memset(Job.Target, 0xFF, sizeof(Job.Target));

    uint32_t NonceBase = (CoreId == 0) ? 0x00000000 : 0x80000000;

    while (true) {
        Job.NonceStart = NonceBase;
        Job.NonceEnd = NonceBase + 4095;
        NonceBase += 4096;

        uint32_t Nonce;
        bool Found;
        PdqSha256MineBlock(&Job, &Nonce, &Found);

        LocalHashes += 4096;

        uint32_t Now = millis();
        if (Now - LastReport >= 1000) {
            g_TotalHashes += LocalHashes;
            if (CoreId == 0) {
                g_HashRate = LocalHashes;
            } else {
                g_HashRate += LocalHashes;
            }
            LocalHashes = 0;
            LastReport = Now;
        }

        esp_task_wdt_reset();
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n");
    Serial.println("╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║           PDQminer Hashrate Benchmark v0.1.0              ║");
    Serial.println("╚═══════════════════════════════════════════════════════════╝");
    Serial.println();

    PdqHalInit();

    Serial.printf("CPU Frequency: %lu MHz\n", PdqHalGetCpuFreqMhz());
    Serial.printf("Chip ID: %08X\n", PdqHalGetChipId());
    Serial.printf("Free Heap: %lu bytes\n", PdqHalGetFreeHeap());
    Serial.println();

    Serial.println("Running single-core SHA256d benchmark (10 seconds)...");
    
    uint8_t Hash[32];
    uint32_t Count = 0;
    uint32_t Start = millis();
    while (millis() - Start < 10000) {
        PdqSha256d(TEST_BLOCK, 80, Hash);
        Count++;
        if (Count % 10000 == 0) {
            esp_task_wdt_reset();
        }
    }
    uint32_t Elapsed = millis() - Start;
    float SingleKHs = (float)Count / (float)Elapsed;
    Serial.printf("Single SHA256d: %.2f KH/s\n\n", SingleKHs);

    Serial.println("Running single-core mining benchmark (10 seconds)...");

    PdqMiningJob_t Job;
    PdqSha256Midstate(TEST_BLOCK, Job.Midstate);
    memcpy(Job.BlockTail, TEST_BLOCK + 64, 16);
    memset(Job.BlockTail + 16, 0x80, 1);
    memset(Job.BlockTail + 17, 0, 39);
    Job.BlockTail[62] = 0x02;
    Job.BlockTail[63] = 0x80;
    memset(Job.Target, 0xFF, sizeof(Job.Target));

    Count = 0;
    Start = millis();
    uint32_t Nonce = 0;
    while (millis() - Start < 10000) {
        Job.NonceStart = Nonce;
        Job.NonceEnd = Nonce + 4095;
        Nonce += 4096;

        uint32_t FoundNonce;
        bool Found;
        PdqSha256MineBlock(&Job, &FoundNonce, &Found);
        Count += 4096;

        if (Count % 100000 < 4096) {
            esp_task_wdt_reset();
        }
    }
    Elapsed = millis() - Start;
    float MiningKHs = (float)Count / (float)Elapsed;
    Serial.printf("Single-core mining: %.2f KH/s\n\n", MiningKHs);

    Serial.println("Starting dual-core mining benchmark...");
    Serial.println("(Results will be reported every 5 seconds)\n");

    xTaskCreatePinnedToCore(BenchmarkTask, "Bench0", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(BenchmarkTask, "Bench1", 4096, NULL, 5, NULL, 1);
}

void loop() {
    static uint32_t LastReport = 0;
    static uint64_t LastTotal = 0;

    if (millis() - LastReport >= 5000) {
        uint64_t Total = g_TotalHashes;
        uint32_t Rate = g_HashRate;
        
        float KHs = (float)Rate / 1000.0f;
        float AvgKHs = (float)(Total - LastTotal) / 5000.0f;

        Serial.println("┌─────────────────────────────────────────┐");
        Serial.printf("│ Current Hashrate: %8.2f KH/s         │\n", KHs);
        Serial.printf("│ Average (5s):     %8.2f KH/s         │\n", AvgKHs);
        Serial.printf("│ Total Hashes:     %12llu          │\n", Total);
        Serial.printf("│ Free Heap:        %8lu bytes       │\n", PdqHalGetFreeHeap());
        Serial.println("└─────────────────────────────────────────┘");
        Serial.println();

        LastReport = millis();
        LastTotal = Total;
        g_HashRate = 0;
    }

    delay(100);
}
