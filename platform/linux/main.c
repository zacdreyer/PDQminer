/**
 * @file main.c
 * @brief Linux entry point for PDQminer
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 *
 * Replaces Arduino setup()/loop() with standard main().
 * Accepts configuration via CLI args or environment variables.
 */

#include "pdq_types.h"
#include "hal/board_hal.h"
#include "config/config_manager.h"
#include "network/wifi_manager.h"
#include "stratum/stratum_client.h"
#include "core/mining_task.h"
#include "core/sha256_engine.h"
#include "api/device_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

/* Defined in linux_mining.c */
extern void PdqMiningSetThreadCount(int n);

static volatile int s_Running = 1;

static void SignalHandler(int sig) {
    (void)sig;
    printf("\n[PDQminer] Shutting down...\n");
    s_Running = 0;
}

static void PrintUsage(const char* prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --pool-host HOST   Stratum pool host (default: pool.nerdminers.org)\n");
    printf("  --pool-port PORT   Stratum pool port (default: 3333)\n");
    printf("  --wallet ADDR      Bitcoin wallet address (required)\n");
    printf("  --worker NAME      Worker name (default: pdqlinux)\n");
    printf("  --threads N        Mining threads (default: 2)\n");
    printf("  --difficulty D     Suggested difficulty (default: 1.0)\n");
    printf("  --config FILE      JSON config file path\n");
    printf("  --help             Show this help\n");
    printf("\nEnvironment variables (override defaults, overridden by CLI):\n");
    printf("  PDQ_POOL_HOST, PDQ_POOL_PORT, PDQ_WALLET, PDQ_WORKER,\n");
    printf("  PDQ_THREADS, PDQ_DIFFICULTY\n");
}

static const char* EnvOr(const char* env, const char* fallback) {
    const char* v = getenv(env);
    return (v && v[0]) ? v : fallback;
}

static uint64_t GetMillis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

int main(int argc, char* argv[]) {
    /* Defaults from env vars, then hardcoded fallbacks */
    char poolHost[PDQ_MAX_HOST_LEN + 1];
    uint16_t poolPort;
    char wallet[PDQ_MAX_WALLET_LEN + 1];
    char worker[PDQ_MAX_WORKER_LEN + 1];
    int threads;
    double difficulty;
    const char* configFile = NULL;

    snprintf(poolHost, sizeof(poolHost), "%s", EnvOr("PDQ_POOL_HOST", "pool.nerdminers.org"));
    poolPort = (uint16_t)atoi(EnvOr("PDQ_POOL_PORT", "3333"));
    snprintf(wallet, sizeof(wallet), "%s", EnvOr("PDQ_WALLET", ""));
    snprintf(worker, sizeof(worker), "%s", EnvOr("PDQ_WORKER", "pdqlinux"));
    threads = atoi(EnvOr("PDQ_THREADS", "2"));
    difficulty = atof(EnvOr("PDQ_DIFFICULTY", "1.0"));

    /* Parse CLI args */
    static struct option longOpts[] = {
        {"pool-host",   required_argument, 0, 'H'},
        {"pool-port",   required_argument, 0, 'P'},
        {"wallet",      required_argument, 0, 'w'},
        {"worker",      required_argument, 0, 'W'},
        {"threads",     required_argument, 0, 't'},
        {"difficulty",  required_argument, 0, 'd'},
        {"config",      required_argument, 0, 'c'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "H:P:w:W:t:d:c:h", longOpts, NULL)) != -1) {
        switch (opt) {
            case 'H': snprintf(poolHost, sizeof(poolHost), "%s", optarg); break;
            case 'P': poolPort = (uint16_t)atoi(optarg); break;
            case 'w': snprintf(wallet, sizeof(wallet), "%s", optarg); break;
            case 'W': snprintf(worker, sizeof(worker), "%s", optarg); break;
            case 't': threads = atoi(optarg); break;
            case 'd': difficulty = atof(optarg); break;
            case 'c': configFile = optarg; break;
            case 'h':
                PrintUsage(argv[0]);
                return 0;
            default:
                PrintUsage(argv[0]);
                return 1;
        }
    }

    /* If --config specified, set env for linux_config.c to pick up */
    if (configFile) {
        setenv("PDQ_CONFIG_PATH", configFile, 1);
    }

    /* Validate required params */
    if (wallet[0] == '\0') {
        fprintf(stderr, "Error: --wallet is required (or set PDQ_WALLET env var)\n\n");
        PrintUsage(argv[0]);
        return 1;
    }
    if (threads < 1) threads = 1;
    if (threads > 32) threads = 32;
    if (poolPort == 0) poolPort = 3333;

    /* ---- Startup banner ---- */
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    printf("===========================================\n");
    printf("  PDQminer v%d.%d.%d (Linux)\n",
           PDQ_VERSION_MAJOR, PDQ_VERSION_MINOR, PDQ_VERSION_PATCH);
    printf("===========================================\n");
    printf("  Pool:       %s:%u\n", poolHost, poolPort);
    printf("  Wallet:     %s\n", wallet);
    printf("  Worker:     %s\n", worker);
    printf("  Threads:    %d\n", threads);
    printf("  Difficulty: %.1f\n", difficulty);
    printf("===========================================\n\n");

    /* ---- Init subsystems ---- */
    PdqHalInit();
    printf("[PDQminer] CPU: %lu MHz, Chip ID: %08X\n",
           (unsigned long)PdqHalGetCpuFreqMhz(), PdqHalGetChipId());

    PdqConfigInit();

    /* Populate config struct from CLI args */
    PdqDeviceConfig_t config;
    memset(&config, 0, sizeof(config));
    snprintf(config.PrimaryPool.Host, sizeof(config.PrimaryPool.Host), "%s", poolHost);
    config.PrimaryPool.Port = poolPort;
    snprintf(config.WalletAddress, sizeof(config.WalletAddress), "%s", wallet);
    snprintf(config.WorkerName, sizeof(config.WorkerName), "%s", worker);

    /* WiFi stub — just sets "connected" state */
    PdqWifiInit();
    PdqWifiConnect("", "");

    /* ---- Stratum connection ---- */
    PdqStratumInit();
    printf("[PDQminer] Connecting to %s:%u...\n", poolHost, poolPort);

    if (PdqStratumConnect(poolHost, poolPort) != PdqOk) {
        fprintf(stderr, "[PDQminer] Pool connection failed\n");
        return 1;
    }
    printf("[PDQminer] Connected to pool\n");

    PdqStratumSubscribe();

    /* Wait for subscribe */
    uint64_t startWait = GetMillis();
    while (PdqStratumGetState() != StratumStateSubscribed && s_Running) {
        PdqStratumProcess();
        if (GetMillis() - startWait > 30000) {
            fprintf(stderr, "[PDQminer] Subscribe timeout\n");
            return 1;
        }
        usleep(100000);
    }
    if (!s_Running) return 0;

    uint8_t extranonce1[PDQ_STRATUM_MAX_EXTRANONCE_LEN];
    uint8_t extranonce1Len = 0;
    PdqStratumGetExtranonce(extranonce1, &extranonce1Len);

    PdqStratumSuggestDifficulty(difficulty);

    /* Build worker string: "wallet.worker" */
    char workerFull[PDQ_MAX_WALLET_LEN + 1 + PDQ_MAX_WORKER_LEN + 1];
    snprintf(workerFull, sizeof(workerFull), "%s.%s", wallet, worker);
    PdqStratumAuthorize(workerFull, "x");

    startWait = GetMillis();
    while (PdqStratumGetState() != StratumStateAuthorized &&
           PdqStratumGetState() != StratumStateReady && s_Running) {
        PdqStratumProcess();
        if (GetMillis() - startWait > 30000) {
            fprintf(stderr, "[PDQminer] Authorize timeout\n");
            return 1;
        }
        usleep(100000);
    }
    if (!s_Running) return 0;
    printf("[PDQminer] Authorized\n");

    /* ---- Start mining ---- */
    PdqMiningSetThreadCount(threads);
    PdqMiningInit();
    PdqMiningStart();

    PdqApiInit();
    PdqApiStart();

    printf("[PDQminer] Mining started with %d thread(s)\n\n", threads);

    /* ---- Main loop ---- */
    uint32_t extranonce2 = 0;
    PdqStratumJob_t stratumJob;
    PdqMinerStats_t stats;
    uint64_t lastPrint = 0;

    while (s_Running) {
        PdqStratumProcess();
        PdqApiProcess();

        /* Check for new mining jobs */
        if (PdqStratumHasNewJob()) {
            PdqStratumGetJob(&stratumJob);

            if (stratumJob.CleanJobs) {
                PdqMiningClearShares();
            }

            PdqMiningJob_t job;
            extranonce2++;

            double poolDiff = PdqStratumGetDifficulty();

            PdqStratumBuildMiningJob(&stratumJob,
                                     extranonce1, extranonce1Len,
                                     extranonce2, PdqStratumGetExtranonce2Size(),
                                     poolDiff,
                                     &job);

            job.NonceStart = 0;
            job.NonceEnd = 0xFFFFFFFF;
            PdqMiningSetJob(&job);
            printf("[PDQminer] New job: %s (diff=%.1f)\n", job.JobId, poolDiff);
        }

        /* Submit found shares */
        if (PdqStratumIsReady()) {
            int sharesThisLoop = 0;
            while (PdqMiningHasShare() && sharesThisLoop < 5) {
                PdqShareInfo_t share;
                if (PdqMiningGetShare(&share) == PdqOk) {
                    PdqStratumSubmitShare(share.JobId, share.Extranonce2,
                                          share.Nonce, share.NTime);
                    printf("[PDQminer] Share submitted: nonce=%08X\n", share.Nonce);
                }
                sharesThisLoop++;
            }
        }

        /* Stats logging */
        PdqMiningGetStats(&stats);
        uint64_t now = GetMillis();
        if (now - lastPrint > 10000) {
            printf("[PDQminer] Hashrate: %lu KH/s | Shares: %lu | Blocks: %lu | Uptime: %lus\n",
                   (unsigned long)(stats.HashRate / 1000),
                   (unsigned long)stats.SharesAccepted,
                   (unsigned long)stats.BlocksFound,
                   (unsigned long)stats.Uptime);
            lastPrint = now;
        }

        PdqHalFeedWdt();
        usleep(10000); /* 10ms */
    }

    /* ---- Shutdown ---- */
    printf("[PDQminer] Stopping mining...\n");
    PdqMiningStop();
    PdqStratumDisconnect();
    PdqApiStop();

    printf("[PDQminer] Shutdown complete.\n");
    return 0;
}
