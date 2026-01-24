/**
 * @file test_sha256_benchmark.cpp
 * @brief SHA256 performance benchmark for baseline measurement
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include <unity.h>
#include "../src/core/sha256_engine.h"
#include "../src/pdq_types.h"

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#define GET_MICROS() esp_timer_get_time()
#else
#include <chrono>
static uint64_t GET_MICROS() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}
#endif

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

void test_sha256_single_hash_performance(void) {
    uint8_t Hash[32];
    const uint32_t Iterations = 10000;

    uint64_t Start = GET_MICROS();
    for (uint32_t i = 0; i < Iterations; i++) {
        PdqSha256(TEST_BLOCK, 80, Hash);
    }
    uint64_t End = GET_MICROS();

    uint64_t ElapsedUs = End - Start;
    double HashesPerSec = (double)Iterations * 1000000.0 / (double)ElapsedUs;
    double KHs = HashesPerSec / 1000.0;

    printf("\n[SHA256 Single] %lu iterations in %llu us\n", Iterations, ElapsedUs);
    printf("[SHA256 Single] %.2f KH/s\n", KHs);

    TEST_ASSERT_TRUE(KHs > 10.0);
}

void test_sha256d_double_hash_performance(void) {
    uint8_t Hash[32];
    const uint32_t Iterations = 10000;

    uint64_t Start = GET_MICROS();
    for (uint32_t i = 0; i < Iterations; i++) {
        PdqSha256d(TEST_BLOCK, 80, Hash);
    }
    uint64_t End = GET_MICROS();

    uint64_t ElapsedUs = End - Start;
    double HashesPerSec = (double)Iterations * 1000000.0 / (double)ElapsedUs;
    double KHs = HashesPerSec / 1000.0;

    printf("\n[SHA256d] %lu iterations in %llu us\n", Iterations, ElapsedUs);
    printf("[SHA256d] %.2f KH/s\n", KHs);

    TEST_ASSERT_TRUE(KHs > 5.0);
}

void test_mining_with_midstate_performance(void) {
    PdqMiningJob_t Job;
    
    PdqSha256Midstate(TEST_BLOCK, Job.Midstate);
    memcpy(Job.BlockTail, TEST_BLOCK + 64, 16);
    memset(Job.BlockTail + 16, 0x80, 1);
    memset(Job.BlockTail + 17, 0, 39);
    Job.BlockTail[62] = 0x02;
    Job.BlockTail[63] = 0x80;
    
    memset(Job.Target, 0xFF, sizeof(Job.Target));
    
    const uint32_t NonceCount = 100000;
    Job.NonceStart = 0;
    Job.NonceEnd = NonceCount - 1;

    uint32_t Nonce;
    bool Found;

    uint64_t Start = GET_MICROS();
    PdqSha256MineBlock(&Job, &Nonce, &Found);
    uint64_t End = GET_MICROS();

    uint64_t ElapsedUs = End - Start;
    double HashesPerSec = (double)NonceCount * 1000000.0 / (double)ElapsedUs;
    double KHs = HashesPerSec / 1000.0;

    printf("\n[Mining w/ Midstate] %lu nonces in %llu us\n", NonceCount, ElapsedUs);
    printf("[Mining w/ Midstate] %.2f KH/s (single core)\n", KHs);
    printf("[Mining w/ Midstate] Estimated dual-core: %.2f KH/s\n", KHs * 1.95);

    TEST_ASSERT_TRUE(KHs > 50.0);
}

void test_sha256_correctness(void) {
    const uint8_t Expected[32] = {
        0x6f, 0xe2, 0x8c, 0x0a, 0xb6, 0xf1, 0xb3, 0x72,
        0xc1, 0xa6, 0xa2, 0x46, 0xae, 0x63, 0xf7, 0x4f,
        0x93, 0x1e, 0x83, 0x65, 0xe1, 0x5a, 0x08, 0x9c,
        0x68, 0xd6, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    uint8_t Hash[32];
    PdqSha256d(TEST_BLOCK, 80, Hash);

    uint8_t Reversed[32];
    for (int i = 0; i < 32; i++) {
        Reversed[i] = Hash[31 - i];
    }

    printf("\n[Correctness] Block hash: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", Reversed[i]);
    }
    printf("\n");

    TEST_ASSERT_EQUAL_MEMORY(Expected, Hash, 32);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_sha256_correctness);
    RUN_TEST(test_sha256_single_hash_performance);
    RUN_TEST(test_sha256d_double_hash_performance);
    RUN_TEST(test_mining_with_midstate_performance);
    
    return UNITY_END();
}
