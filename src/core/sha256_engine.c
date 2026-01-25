/**
 * @file sha256_engine.c
 * @brief High-performance SHA256 engine optimized for Bitcoin mining
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "sha256_engine.h"
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PDQ_IRAM_ATTR IRAM_ATTR
#define PDQ_DRAM_ATTR DRAM_ATTR
#else
#define PDQ_IRAM_ATTR
#define PDQ_DRAM_ATTR
#endif

static PDQ_DRAM_ATTR const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const uint32_t H_INIT[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

#define SHA256_ROUND(a, b, c, d, e, f, g, h, k, w) do { \
    uint32_t t1 = (h) + EP1(e) + CH(e, f, g) + (k) + (w); \
    uint32_t t2 = EP0(a) + MAJ(a, b, c); \
    (h) = (g); (g) = (f); (f) = (e); (e) = (d) + t1; \
    (d) = (c); (c) = (b); (b) = (a); (a) = t1 + t2; \
} while(0)

static inline uint32_t ReadBe32(const uint8_t* p_Data) {
    return ((uint32_t)p_Data[0] << 24) | ((uint32_t)p_Data[1] << 16) |
           ((uint32_t)p_Data[2] << 8) | (uint32_t)p_Data[3];
}

static inline void WriteBe32(uint8_t* p_Data, uint32_t Value) {
    p_Data[0] = (uint8_t)(Value >> 24);
    p_Data[1] = (uint8_t)(Value >> 16);
    p_Data[2] = (uint8_t)(Value >> 8);
    p_Data[3] = (uint8_t)Value;
}

static inline void WriteLe32(uint8_t* p_Data, uint32_t Value) {
    p_Data[0] = (uint8_t)Value;
    p_Data[1] = (uint8_t)(Value >> 8);
    p_Data[2] = (uint8_t)(Value >> 16);
    p_Data[3] = (uint8_t)(Value >> 24);
}

PDQ_IRAM_ATTR static void Sha256Transform(uint32_t* p_State, const uint8_t* p_Block) {
    register uint32_t a, b, c, d, e, f, g, h;
    uint32_t W[64];

    for (int i = 0; i < 16; i++) {
        W[i] = ReadBe32(p_Block + i * 4);
    }

    for (int i = 16; i < 64; i++) {
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];
    }

    a = p_State[0]; b = p_State[1]; c = p_State[2]; d = p_State[3];
    e = p_State[4]; f = p_State[5]; g = p_State[6]; h = p_State[7];

    SHA256_ROUND(a,b,c,d,e,f,g,h, K[0], W[0]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[1], W[1]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[2], W[2]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[3], W[3]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[4], W[4]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[5], W[5]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[6], W[6]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[7], W[7]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[8], W[8]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[9], W[9]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[10], W[10]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[11], W[11]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[12], W[12]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[13], W[13]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[14], W[14]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[15], W[15]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[16], W[16]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[17], W[17]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[18], W[18]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[19], W[19]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[20], W[20]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[21], W[21]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[22], W[22]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[23], W[23]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[24], W[24]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[25], W[25]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[26], W[26]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[27], W[27]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[28], W[28]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[29], W[29]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[30], W[30]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[31], W[31]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[32], W[32]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[33], W[33]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[34], W[34]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[35], W[35]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[36], W[36]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[37], W[37]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[38], W[38]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[39], W[39]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[40], W[40]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[41], W[41]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[42], W[42]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[43], W[43]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[44], W[44]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[45], W[45]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[46], W[46]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[47], W[47]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[48], W[48]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[49], W[49]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[50], W[50]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[51], W[51]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[52], W[52]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[53], W[53]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[54], W[54]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[55], W[55]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[56], W[56]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[57], W[57]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[58], W[58]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[59], W[59]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[60], W[60]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[61], W[61]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[62], W[62]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[63], W[63]);

    p_State[0] += a; p_State[1] += b; p_State[2] += c; p_State[3] += d;
    p_State[4] += e; p_State[5] += f; p_State[6] += g; p_State[7] += h;
}

PdqError_t PdqSha256Init(PdqSha256Context_t* p_Ctx) {
    if (p_Ctx == NULL) return PdqErrorInvalidParam;
    memcpy(p_Ctx->State, H_INIT, sizeof(H_INIT));
    p_Ctx->ByteCount = 0;
    return PdqOk;
}

PdqError_t PdqSha256Update(PdqSha256Context_t* p_Ctx, const uint8_t* p_Data, size_t Length) {
    if (p_Ctx == NULL || (p_Data == NULL && Length > 0)) return PdqErrorInvalidParam;

    size_t BufferIdx = p_Ctx->ByteCount % 64;
    p_Ctx->ByteCount += Length;

    if (BufferIdx > 0) {
        size_t ToCopy = 64 - BufferIdx;
        if (ToCopy > Length) ToCopy = Length;
        memcpy(p_Ctx->Buffer + BufferIdx, p_Data, ToCopy);
        p_Data += ToCopy;
        Length -= ToCopy;
        BufferIdx += ToCopy;
        if (BufferIdx == 64) {
            Sha256Transform(p_Ctx->State, p_Ctx->Buffer);
        }
    }

    while (Length >= 64) {
        Sha256Transform(p_Ctx->State, p_Data);
        p_Data += 64;
        Length -= 64;
    }

    if (Length > 0) {
        memcpy(p_Ctx->Buffer, p_Data, Length);
    }

    return PdqOk;
}

PdqError_t PdqSha256Final(PdqSha256Context_t* p_Ctx, uint8_t* p_Hash) {
    if (p_Ctx == NULL || p_Hash == NULL) return PdqErrorInvalidParam;

    size_t BufferIdx = p_Ctx->ByteCount % 64;
    p_Ctx->Buffer[BufferIdx++] = 0x80;

    if (BufferIdx > 56) {
        memset(p_Ctx->Buffer + BufferIdx, 0, 64 - BufferIdx);
        Sha256Transform(p_Ctx->State, p_Ctx->Buffer);
        BufferIdx = 0;
    }

    memset(p_Ctx->Buffer + BufferIdx, 0, 56 - BufferIdx);
    uint64_t BitLen = (uint64_t)p_Ctx->ByteCount * 8;
    for (int i = 0; i < 8; i++) {
        p_Ctx->Buffer[63 - i] = (uint8_t)(BitLen >> (i * 8));
    }
    Sha256Transform(p_Ctx->State, p_Ctx->Buffer);

    for (int i = 0; i < 8; i++) {
        WriteBe32(p_Hash + i * 4, p_Ctx->State[i]);
    }

    return PdqOk;
}

PdqError_t PdqSha256(const uint8_t* p_Data, size_t Length, uint8_t* p_Hash) {
    PdqSha256Context_t Ctx;
    PdqSha256Init(&Ctx);
    PdqSha256Update(&Ctx, p_Data, Length);
    return PdqSha256Final(&Ctx, p_Hash);
}

PdqError_t PdqSha256d(const uint8_t* p_Data, size_t Length, uint8_t* p_Hash) {
    uint8_t FirstHash[32];
    PdqSha256(p_Data, Length, FirstHash);
    return PdqSha256(FirstHash, 32, p_Hash);
}

PdqError_t PdqSha256Midstate(const uint8_t* p_BlockHeader, uint8_t* p_Midstate) {
    if (p_BlockHeader == NULL || p_Midstate == NULL) return PdqErrorInvalidParam;

    uint32_t State[8];
    memcpy(State, H_INIT, sizeof(H_INIT));
    Sha256Transform(State, p_BlockHeader);

    for (int i = 0; i < 8; i++) {
        WriteBe32(p_Midstate + i * 4, State[i]);
    }

    return PdqOk;
}

PDQ_IRAM_ATTR static bool CheckTarget(const uint32_t* p_Hash, const uint32_t* p_Target) {
    for (int i = 7; i >= 0; i--) {
        if (p_Hash[i] > p_Target[i]) return false;
        if (p_Hash[i] < p_Target[i]) return true;
    }
    return true;
}

PDQ_IRAM_ATTR PdqError_t PdqSha256MineBlock(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    if (p_Job == NULL || p_Nonce == NULL || p_Found == NULL) return PdqErrorInvalidParam;

    *p_Found = false;

    uint32_t MidState[8];
    for (int i = 0; i < 8; i++) {
        MidState[i] = ReadBe32(p_Job->Midstate + i * 4);
    }

    uint8_t Block[64];
    memcpy(Block, p_Job->BlockTail, 64);

    for (uint32_t Nonce = p_Job->NonceStart; Nonce <= p_Job->NonceEnd; Nonce++) {
        WriteLe32(Block + 12, Nonce);

        uint32_t State[8];
        memcpy(State, MidState, sizeof(MidState));
        Sha256Transform(State, Block);

        uint8_t FirstHash[32];
        for (int i = 0; i < 8; i++) {
            WriteBe32(FirstHash + i * 4, State[i]);
        }

        uint32_t FinalState[8];
        memcpy(FinalState, H_INIT, sizeof(H_INIT));

        uint8_t PaddedHash[64];
        memcpy(PaddedHash, FirstHash, 32);
        PaddedHash[32] = 0x80;
        memset(PaddedHash + 33, 0, 31);
        PaddedHash[62] = 0x01;

        Sha256Transform(FinalState, PaddedHash);

        if (CheckTarget(FinalState, p_Job->Target)) {
            *p_Nonce = Nonce;
            *p_Found = true;
            return PdqOk;
        }
    }

    return PdqOk;
}
