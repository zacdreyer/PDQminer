/**
 * @file linux_hal.c
 * @brief Linux implementation of board HAL
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "hal/board_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#if defined(__linux__)
#include <sys/sysinfo.h>
#endif

PdqError_t PdqHalInit(void) {
    return PdqOk;
}

uint32_t PdqHalGetCpuFreqMhz(void) {
#if defined(__linux__)
    FILE* f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (f) {
        unsigned long khz = 0;
        if (fscanf(f, "%lu", &khz) == 1) {
            fclose(f);
            return (uint32_t)(khz / 1000);
        }
        fclose(f);
    }
#endif
    return 0;
}

float PdqHalGetTemperature(void) {
#if defined(__linux__)
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int millideg = 0;
        if (fscanf(f, "%d", &millideg) == 1) {
            fclose(f);
            return millideg / 1000.0f;
        }
        fclose(f);
    }
#endif
    return 0.0f;
}

uint32_t PdqHalGetFreeHeap(void) {
#if defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (uint32_t)(si.freeram & 0xFFFFFFFF);
    }
#endif
    return 0;
}

uint32_t PdqHalGetChipId(void) {
    /* Derive a stable ID from hostname */
    char hostname[64] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        uint32_t hash = 5381;
        for (int i = 0; hostname[i]; i++) {
            hash = ((hash << 5) + hash) + (uint8_t)hostname[i];
        }
        return hash & 0x00FFFFFF;
    }
    return 0x004C4E58; /* "LNX" */
}

void PdqHalFeedWdt(void) {
    /* No watchdog on Linux */
}

void PdqHalRestart(void) {
    printf("[PDQminer] Restart requested, exiting.\n");
    exit(0);
}
