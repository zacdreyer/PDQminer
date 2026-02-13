/**
 * @file mining_task.c
 * @brief Mining task implementation with ESP32/ESP8266 support
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "mining_task.h"
#include "sha256_engine.h"
#include <string.h>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"
#define PDQ_IRAM_ATTR IRAM_ATTR
#define PDQ_USE_RTOS 1
#define PDQ_MINING_STACK_SIZE   8192
#define PDQ_MINING_PRIORITY     5
#define PDQ_NONCE_BATCH_SIZE    8192
#define PDQ_WDT_FEED_INTERVAL   1000
#else
#define PDQ_IRAM_ATTR
#define PDQ_USE_RTOS 0
#define PDQ_NONCE_BATCH_SIZE    4096
#define PDQ_WDT_FEED_INTERVAL   1000
#endif

#define PDQ_SHARE_QUEUE_SIZE    8

typedef struct {
    volatile bool           Running;
    volatile bool           HasJob;
    volatile uint32_t       JobVersion;
    volatile uint64_t       TotalHashes;
    volatile uint32_t       HashRate;
    volatile uint32_t       SharesAccepted;
    volatile uint32_t       SharesRejected;
    volatile uint32_t       BlocksFound;
    uint32_t                StartTime;
    PdqMiningJob_t          CurrentJob;
    uint32_t                CurrentNonce;
    uint32_t                LastWdtFeed;
    uint32_t                LastHashReport;
    uint64_t                LocalHashes;
#if PDQ_USE_RTOS
    TaskHandle_t            TaskHandle[2];
    SemaphoreHandle_t       JobMutex;
    QueueHandle_t           ShareQueue;
#else
    PdqShareInfo_t          ShareBuffer[PDQ_SHARE_QUEUE_SIZE];
    volatile uint8_t        ShareHead;
    volatile uint8_t        ShareTail;
#endif
} MiningState_t;

static MiningState_t s_State = {0};

#if PDQ_USE_RTOS
static void QueueShare(const PdqMiningJob_t* p_Job, uint32_t Nonce) {
    PdqShareInfo_t Share;
    strncpy(Share.JobId, p_Job->JobId, 64);
    Share.JobId[64] = '\0';
    Share.Extranonce2 = p_Job->Extranonce2;
    Share.Nonce = Nonce;
    Share.NTime = p_Job->NTime;
    xQueueSend(s_State.ShareQueue, &Share, 0);
}

PDQ_IRAM_ATTR static void MiningTaskCore0(void* p_Param) {
    esp_task_wdt_add(NULL);
    uint32_t LastWdtFeed = 0;
    uint32_t LocalHashes = 0;
    uint32_t LastHashReport = 0;

    while (s_State.Running) {
        if (!s_State.HasJob) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        xSemaphoreTake(s_State.JobMutex, portMAX_DELAY);
        PdqMiningJob_t Job = s_State.CurrentJob;
        uint32_t MyJobVersion = s_State.JobVersion;
        Job.NonceStart = 0x00000000;
        Job.NonceEnd = 0x7FFFFFFF;
        xSemaphoreGive(s_State.JobMutex);

        for (uint32_t Base = Job.NonceStart; Base <= Job.NonceEnd && s_State.Running && s_State.JobVersion == MyJobVersion; Base += PDQ_NONCE_BATCH_SIZE) {
            PdqMiningJob_t BatchJob = Job;
            BatchJob.NonceStart = Base;
            BatchJob.NonceEnd = Base + PDQ_NONCE_BATCH_SIZE - 1;
            if (BatchJob.NonceEnd > Job.NonceEnd) BatchJob.NonceEnd = Job.NonceEnd;

            uint32_t Nonce;
            bool Found;
            PdqSha256MineBlock(&BatchJob, &Nonce, &Found);

            LocalHashes += (BatchJob.NonceEnd - BatchJob.NonceStart + 1);

            if (Found) {
                QueueShare(&Job, Nonce);
                __atomic_fetch_add(&s_State.BlocksFound, 1, __ATOMIC_RELAXED);
            }

            uint32_t Now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (Now - LastWdtFeed > PDQ_WDT_FEED_INTERVAL) {
                esp_task_wdt_reset();
                vTaskDelay(1);
                LastWdtFeed = Now;
            }

            if (Now - LastHashReport > 1000) {
                __atomic_fetch_add(&s_State.TotalHashes, LocalHashes, __ATOMIC_RELAXED);
                LocalHashes = 0;
                LastHashReport = Now;
            }
        }
    }

    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

PDQ_IRAM_ATTR static void MiningTaskCore1(void* p_Param) {
    esp_task_wdt_add(NULL);
    uint32_t LastWdtFeed = 0;
    uint32_t LocalHashes = 0;
    uint32_t LastHashReport = 0;

    while (s_State.Running) {
        if (!s_State.HasJob) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        xSemaphoreTake(s_State.JobMutex, portMAX_DELAY);
        PdqMiningJob_t Job = s_State.CurrentJob;
        uint32_t MyJobVersion = s_State.JobVersion;
        Job.NonceStart = 0x80000000;
        Job.NonceEnd = 0xFFFFFFFF;
        xSemaphoreGive(s_State.JobMutex);

        for (uint32_t Base = Job.NonceStart; Base <= Job.NonceEnd && s_State.Running && s_State.JobVersion == MyJobVersion; Base += PDQ_NONCE_BATCH_SIZE) {
            PdqMiningJob_t BatchJob = Job;
            BatchJob.NonceStart = Base;
            BatchJob.NonceEnd = Base + PDQ_NONCE_BATCH_SIZE - 1;
            if (BatchJob.NonceEnd > Job.NonceEnd || BatchJob.NonceEnd < Base) BatchJob.NonceEnd = Job.NonceEnd;

            uint32_t Nonce;
            bool Found;
            PdqSha256MineBlock(&BatchJob, &Nonce, &Found);

            LocalHashes += (BatchJob.NonceEnd - BatchJob.NonceStart + 1);

            if (Found) {
                QueueShare(&Job, Nonce);
                __atomic_fetch_add(&s_State.BlocksFound, 1, __ATOMIC_RELAXED);
            }

            uint32_t Now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (Now - LastWdtFeed > PDQ_WDT_FEED_INTERVAL) {
                esp_task_wdt_reset();
                vTaskDelay(1);
                LastWdtFeed = Now;
            }

            if (Now - LastHashReport > 1000) {
                __atomic_fetch_add(&s_State.TotalHashes, LocalHashes, __ATOMIC_RELAXED);
                LocalHashes = 0;
                LastHashReport = Now;
            }
        }
    }

    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}
#else
static void QueueShareNonRtos(const PdqMiningJob_t* p_Job, uint32_t Nonce) {
    uint8_t NextHead = (s_State.ShareHead + 1) % PDQ_SHARE_QUEUE_SIZE;
    if (NextHead != s_State.ShareTail) {
        PdqShareInfo_t* Share = &s_State.ShareBuffer[s_State.ShareHead];
        strncpy(Share->JobId, p_Job->JobId, 64);
        Share->JobId[64] = '\0';
        Share->Extranonce2 = p_Job->Extranonce2;
        Share->Nonce = Nonce;
        Share->NTime = p_Job->NTime;
        s_State.ShareHead = NextHead;
    }
}
#endif

PdqError_t PdqMiningInit(void) {
    memset(&s_State, 0, sizeof(s_State));
#if PDQ_USE_RTOS
    s_State.JobMutex = xSemaphoreCreateMutex();
    if (s_State.JobMutex == NULL) return PdqErrorNoMemory;
    s_State.ShareQueue = xQueueCreate(PDQ_SHARE_QUEUE_SIZE, sizeof(PdqShareInfo_t));
    if (s_State.ShareQueue == NULL) return PdqErrorNoMemory;
#endif
    return PdqOk;
}

PdqError_t PdqMiningStart(void) {
    if (s_State.Running) return PdqOk;

    s_State.Running = true;
    s_State.StartTime = 0;

#if PDQ_USE_RTOS
    s_State.StartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;

    xTaskCreatePinnedToCore(
        MiningTaskCore0,
        "MineCore0",
        PDQ_MINING_STACK_SIZE,
        NULL,
        PDQ_MINING_PRIORITY,
        &s_State.TaskHandle[0],
        0
    );

    xTaskCreatePinnedToCore(
        MiningTaskCore1,
        "MineCore1",
        PDQ_MINING_STACK_SIZE,
        NULL,
        PDQ_MINING_PRIORITY,
        &s_State.TaskHandle[1],
        1
    );
#endif

    return PdqOk;
}

PdqError_t PdqMiningStop(void) {
    s_State.Running = false;
#if PDQ_USE_RTOS
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
    return PdqOk;
}

PdqError_t PdqMiningSetJob(const PdqMiningJob_t* p_Job) {
    if (p_Job == NULL) return PdqErrorInvalidParam;

#if PDQ_USE_RTOS
    xSemaphoreTake(s_State.JobMutex, portMAX_DELAY);
#endif
    memcpy(&s_State.CurrentJob, p_Job, sizeof(PdqMiningJob_t));
    s_State.JobVersion++;
    s_State.HasJob = true;
#if PDQ_USE_RTOS
    xSemaphoreGive(s_State.JobMutex);
#endif

    return PdqOk;
}

PdqError_t PdqMiningGetStats(PdqMinerStats_t* p_Stats) {
    if (p_Stats == NULL) return PdqErrorInvalidParam;

    static uint64_t LastTotalHashes = 0;
    static uint32_t LastStatTime = 0;

#if PDQ_USE_RTOS
    uint32_t Now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t Elapsed = Now - LastStatTime;

    if (Elapsed >= 1000) {
        uint64_t CurrentTotal = s_State.TotalHashes;
        uint64_t DeltaHashes = CurrentTotal - LastTotalHashes;
        s_State.HashRate = (uint32_t)(DeltaHashes * 1000 / Elapsed);
        LastTotalHashes = CurrentTotal;
        LastStatTime = Now;
    }

    p_Stats->Uptime = (Now - s_State.StartTime) / 1000;
#else
    p_Stats->Uptime = 0;
#endif

    p_Stats->HashRate = s_State.HashRate;
    p_Stats->TotalHashes = s_State.TotalHashes;
    p_Stats->SharesAccepted = s_State.SharesAccepted;
    p_Stats->SharesRejected = s_State.SharesRejected;
    p_Stats->BlocksFound = s_State.BlocksFound;
    p_Stats->Temperature = 0.0f;

    return PdqOk;
}

bool PdqMiningIsRunning(void) {
    return s_State.Running;
}

bool PdqMiningHasShare(void) {
#if PDQ_USE_RTOS
    if (s_State.ShareQueue == NULL) return false;
    return uxQueueMessagesWaiting(s_State.ShareQueue) > 0;
#else
    return s_State.ShareHead != s_State.ShareTail;
#endif
}

PdqError_t PdqMiningGetShare(PdqShareInfo_t* p_Share) {
    if (p_Share == NULL) return PdqErrorInvalidParam;
#if PDQ_USE_RTOS
    if (s_State.ShareQueue == NULL) return PdqErrorInvalidParam;
    if (xQueueReceive(s_State.ShareQueue, p_Share, 0) == pdTRUE) {
        return PdqOk;
    }
    return PdqErrorInvalidParam;
#else
    if (s_State.ShareHead == s_State.ShareTail) return PdqErrorInvalidParam;
    *p_Share = s_State.ShareBuffer[s_State.ShareTail];
    s_State.ShareTail = (s_State.ShareTail + 1) % PDQ_SHARE_QUEUE_SIZE;
    return PdqOk;
#endif
}

void PdqMiningClearShares(void) {
#if PDQ_USE_RTOS
    if (s_State.ShareQueue != NULL) {
        xQueueReset(s_State.ShareQueue);
    }
#else
    s_State.ShareHead = s_State.ShareTail = 0;
#endif
}
