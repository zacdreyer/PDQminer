/**
 * @file linux_mining.c
 * @brief Linux pthread implementation of mining_task
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 *
 * Replaces the FreeRTOS dual-core mining_task.c with POSIX threads.
 * Supports N configurable mining threads, each scanning a non-overlapping
 * slice of the 4 GiB nonce space using the software SHA256 path.
 */

#include "core/mining_task.h"
#include "core/sha256_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

#define PDQ_SHARE_QUEUE_SIZE     16
#define PDQ_NONCE_BATCH_SIZE     4096
#define PDQ_MAX_THREADS          32

/* Configurable thread count — set before PdqMiningStart() */
static int s_NumThreads = 2;

void PdqMiningSetThreadCount(int n) {
    if (n < 1) n = 1;
    if (n > PDQ_MAX_THREADS) n = PDQ_MAX_THREADS;
    s_NumThreads = n;
}

typedef struct {
    volatile int            Running;
    volatile int            HasJob;
    atomic_uint             JobVersion;
    atomic_uint_fast64_t    TotalHashes;
    atomic_uint             HashRate;
    atomic_uint             SharesAccepted;
    atomic_uint             SharesRejected;
    atomic_uint             BlocksFound;
    struct timespec         StartTime;
    PdqMiningJob_t          CurrentJob;
    pthread_mutex_t         JobMutex;

    /* Lock-free share ring buffer */
    PdqShareInfo_t          ShareBuffer[PDQ_SHARE_QUEUE_SIZE];
    atomic_uint             ShareHead;
    atomic_uint             ShareTail;

    pthread_t               Threads[PDQ_MAX_THREADS];
    int                     ThreadCount;
} MiningState_t;

static MiningState_t s_State;

static uint64_t GetMillis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void QueueShare(const PdqMiningJob_t* p_Job, uint32_t Nonce) {
    unsigned head = atomic_load(&s_State.ShareHead);
    unsigned next = (head + 1) % PDQ_SHARE_QUEUE_SIZE;
    if (next == atomic_load(&s_State.ShareTail)) {
        printf("[Mining] WARN: Share queue full, dropping share nonce=%08X\n", Nonce);
        return;
    }

    PdqShareInfo_t* s = &s_State.ShareBuffer[head];
    strncpy(s->JobId, p_Job->JobId, 64);
    s->JobId[64] = '\0';
    s->Extranonce2 = p_Job->Extranonce2;
    s->Nonce = Nonce;
    s->NTime = p_Job->NTime;
    atomic_store(&s_State.ShareHead, next);
}

typedef struct {
    int      ThreadIndex;
    uint32_t NonceStart;
    uint32_t NonceEnd;
} ThreadArg_t;

static void* MiningThread(void* arg) {
    ThreadArg_t* ta = (ThreadArg_t*)arg;
    uint32_t myNonceStart = ta->NonceStart;
    uint32_t myNonceEnd = ta->NonceEnd;
    int idx = ta->ThreadIndex;
    free(ta);

    printf("[Mine-%d] Thread started, nonce range %08X-%08X\n",
           idx, myNonceStart, myNonceEnd);

    uint64_t localHashes = 0;
    uint64_t lastReport = GetMillis();

    while (s_State.Running) {
        if (!s_State.HasJob) {
            struct timespec ts = {0, 10000000}; /* 10ms */
            nanosleep(&ts, NULL);
            continue;
        }

        pthread_mutex_lock(&s_State.JobMutex);
        PdqMiningJob_t job = s_State.CurrentJob;
        unsigned myJobVer = atomic_load(&s_State.JobVersion);
        job.NonceStart = myNonceStart;
        job.NonceEnd = myNonceEnd;
        pthread_mutex_unlock(&s_State.JobMutex);

        uint32_t base = myNonceStart;
        while (s_State.Running && atomic_load(&s_State.JobVersion) == myJobVer) {
            job.NonceStart = base;
            uint32_t batchEnd = base + PDQ_NONCE_BATCH_SIZE - 1;
            if (batchEnd > myNonceEnd || batchEnd < base) batchEnd = myNonceEnd;
            job.NonceEnd = batchEnd;

            uint32_t nonce;
            bool found;
            PdqSha256MineBlock(&job, &nonce, &found);

            if (found)
                localHashes += (nonce - job.NonceStart + 1);
            else
                localHashes += (job.NonceEnd - job.NonceStart + 1);

            if (found) {
                QueueShare(&job, nonce);
                atomic_fetch_add(&s_State.BlocksFound, 1);
                printf("[Mine-%d] *** SHARE FOUND *** nonce=%08X\n", idx, nonce);
            }

            uint64_t now = GetMillis();
            if (now - lastReport > 1000) {
                atomic_fetch_add(&s_State.TotalHashes, localHashes);
                localHashes = 0;
                lastReport = now;
            }

            if (batchEnd >= myNonceEnd) break;
            base = batchEnd + 1;
        }
    }

    /* Flush remaining hashes */
    if (localHashes > 0) {
        atomic_fetch_add(&s_State.TotalHashes, localHashes);
    }

    printf("[Mine-%d] Thread exiting\n", idx);
    return NULL;
}

PdqError_t PdqMiningInit(void) {
    memset(&s_State, 0, sizeof(s_State));
    pthread_mutex_init(&s_State.JobMutex, NULL);
    atomic_store(&s_State.ShareHead, 0);
    atomic_store(&s_State.ShareTail, 0);
    atomic_store(&s_State.JobVersion, 0);
    atomic_store(&s_State.TotalHashes, 0);
    atomic_store(&s_State.HashRate, 0);
    atomic_store(&s_State.SharesAccepted, 0);
    atomic_store(&s_State.SharesRejected, 0);
    atomic_store(&s_State.BlocksFound, 0);
    return PdqOk;
}

PdqError_t PdqMiningStart(void) {
    if (s_State.Running) return PdqOk;

    s_State.Running = 1;
    clock_gettime(CLOCK_MONOTONIC, &s_State.StartTime);

    int n = s_NumThreads;
    s_State.ThreadCount = n;

    /* Split 32-bit nonce space evenly across threads */
    uint64_t total = 0x100000000ULL;
    uint64_t perThread = total / (uint64_t)n;

    for (int i = 0; i < n; i++) {
        ThreadArg_t* ta = (ThreadArg_t*)malloc(sizeof(ThreadArg_t));
        if (!ta) return PdqErrorNoMemory;
        ta->ThreadIndex = i;
        ta->NonceStart = (uint32_t)(perThread * (uint64_t)i);
        ta->NonceEnd = (i == n - 1) ? 0xFFFFFFFF : (uint32_t)(perThread * (uint64_t)(i + 1) - 1);

        if (pthread_create(&s_State.Threads[i], NULL, MiningThread, ta) != 0) {
            free(ta);
            return PdqErrorNoMemory;
        }
    }

    printf("[Mining] Started %d thread(s)\n", n);
    return PdqOk;
}

PdqError_t PdqMiningStop(void) {
    s_State.Running = 0;
    for (int i = 0; i < s_State.ThreadCount; i++) {
        pthread_join(s_State.Threads[i], NULL);
    }
    printf("[Mining] All threads stopped\n");
    return PdqOk;
}

PdqError_t PdqMiningSetJob(const PdqMiningJob_t* p_Job) {
    if (!p_Job) return PdqErrorInvalidParam;

    pthread_mutex_lock(&s_State.JobMutex);
    memcpy(&s_State.CurrentJob, p_Job, sizeof(PdqMiningJob_t));
    atomic_fetch_add(&s_State.JobVersion, 1);
    s_State.HasJob = 1;
    pthread_mutex_unlock(&s_State.JobMutex);

    return PdqOk;
}

PdqError_t PdqMiningGetStats(PdqMinerStats_t* p_Stats) {
    if (!p_Stats) return PdqErrorInvalidParam;

    static uint64_t lastTotal = 0;
    static uint64_t lastTime = 0;

    uint64_t now = GetMillis();
    uint64_t elapsed = now - lastTime;

    if (elapsed >= 1000) {
        uint64_t currentTotal = atomic_load(&s_State.TotalHashes);
        uint64_t delta = currentTotal - lastTotal;
        atomic_store(&s_State.HashRate, (unsigned)(delta * 1000 / elapsed));
        lastTotal = currentTotal;
        lastTime = now;
    }

    uint64_t upMs = now - ((uint64_t)s_State.StartTime.tv_sec * 1000 +
                           (uint64_t)s_State.StartTime.tv_nsec / 1000000);

    p_Stats->HashRate = atomic_load(&s_State.HashRate);
    p_Stats->HashRateSw = p_Stats->HashRate;
    p_Stats->HashRateHw = 0;
    p_Stats->TotalHashes = atomic_load(&s_State.TotalHashes);
    p_Stats->SharesAccepted = atomic_load(&s_State.SharesAccepted);
    p_Stats->SharesRejected = atomic_load(&s_State.SharesRejected);
    p_Stats->BlocksFound = atomic_load(&s_State.BlocksFound);
    p_Stats->Uptime = (uint32_t)(upMs / 1000);
    p_Stats->Temperature = 0.0f;
    p_Stats->Difficulty = 0.0;
    p_Stats->Templates = 0;
    p_Stats->BestDiff = 0.0;
    p_Stats->WifiConnected = true;

    return PdqOk;
}

bool PdqMiningIsRunning(void) {
    return s_State.Running != 0;
}

bool PdqMiningHasShare(void) {
    return atomic_load(&s_State.ShareHead) != atomic_load(&s_State.ShareTail);
}

PdqError_t PdqMiningGetShare(PdqShareInfo_t* p_Share) {
    if (!p_Share) return PdqErrorInvalidParam;
    unsigned tail = atomic_load(&s_State.ShareTail);
    if (tail == atomic_load(&s_State.ShareHead)) return PdqErrorInvalidParam;
    *p_Share = s_State.ShareBuffer[tail];
    atomic_store(&s_State.ShareTail, (tail + 1) % PDQ_SHARE_QUEUE_SIZE);
    return PdqOk;
}

void PdqMiningClearShares(void) {
    atomic_store(&s_State.ShareHead, 0);
    atomic_store(&s_State.ShareTail, 0);
}

void PdqMiningPause(void) {
    /* No-op on Linux */
}

void PdqMiningResume(void) {
    /* No-op on Linux */
}
