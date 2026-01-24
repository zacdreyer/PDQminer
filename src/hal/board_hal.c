/**
 * @file board_hal.c
 * @brief Hardware abstraction layer implementation
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "board_hal.h"

#ifdef ESP_PLATFORM
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_mac.h"
#endif

PdqError_t PdqHalInit(void) {
#ifdef ESP_PLATFORM
    esp_task_wdt_init(30, true);
#endif
    return PdqOk;
}

uint32_t PdqHalGetCpuFreqMhz(void) {
    return 240;
}

float PdqHalGetTemperature(void) {
#ifdef ESP_PLATFORM
    extern float temperatureRead(void);
    return temperatureRead();
#else
    return 0.0f;
#endif
}

uint32_t PdqHalGetFreeHeap(void) {
#ifdef ESP_PLATFORM
    return esp_get_free_heap_size();
#else
    return 0;
#endif
}

uint32_t PdqHalGetChipId(void) {
#ifdef ESP_PLATFORM
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (mac[3] << 16) | (mac[4] << 8) | mac[5];
#else
    return 0;
#endif
}

void PdqHalFeedWdt(void) {
#ifdef ESP_PLATFORM
    esp_task_wdt_reset();
#endif
}

void PdqHalRestart(void) {
#ifdef ESP_PLATFORM
    esp_restart();
#endif
}
