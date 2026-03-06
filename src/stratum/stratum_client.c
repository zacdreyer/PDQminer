/**
 * @file stratum_client.c
 * @brief Stratum V1 protocol client implementation
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "stratum_client.h"
#include "core/sha256_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ESP32
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#endif

#define JSON_ID_SUBSCRIBE       1
#define JSON_ID_AUTHORIZE       2
#define JSON_ID_SUGGEST_DIFF    3
#define JSON_ID_SUBMIT_BASE     100

typedef struct {
    PdqStratumState_t State;
    int               Socket;
    char              RecvBuffer[PDQ_STRATUM_RECV_BUFFER_SIZE];
    uint16_t          RecvLen;
    char              SendBuffer[PDQ_STRATUM_SEND_BUFFER_SIZE];
    uint8_t           Extranonce1[PDQ_STRATUM_MAX_EXTRANONCE_LEN];
    uint8_t           Extranonce1Len;
    uint32_t          Extranonce2Size;
    double            Difficulty;
    uint32_t          SubmitId;
    PdqStratumJob_t   CurrentJob;
    bool              HasNewJob;
    char              Worker[PDQ_MAX_WORKER_LEN + 1];
    char              Password[PDQ_MAX_PASSWORD_LEN + 1];
} StratumContext_t;

static StratumContext_t s_Ctx;

static int32_t HexCharToNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int32_t HexToBytes(const char* p_Hex, uint8_t* p_Out, size_t MaxLen)
{
    if (p_Hex == NULL || p_Out == NULL) return -1;
    size_t HexLen = strlen(p_Hex);
    if (HexLen % 2 != 0 || HexLen / 2 > MaxLen) return -1;
    
    for (size_t i = 0; i < HexLen / 2; i++) {
        int32_t Hi = HexCharToNibble(p_Hex[i * 2]);
        int32_t Lo = HexCharToNibble(p_Hex[i * 2 + 1]);
        if (Hi < 0 || Lo < 0) return -1;
        p_Out[i] = (uint8_t)((Hi << 4) | Lo);
    }
    return (int32_t)(HexLen / 2);
}

static void BytesToHex(const uint8_t* p_In, size_t Len, char* p_Out)
{
    static const char Hex[] = "0123456789abcdef";
    for (size_t i = 0; i < Len; i++) {
        p_Out[i * 2] = Hex[p_In[i] >> 4];
        p_Out[i * 2 + 1] = Hex[p_In[i] & 0x0F];
    }
    p_Out[Len * 2] = '\0';
}

static void ReverseBytes(uint8_t* p_Data, size_t Len)
{
    for (size_t i = 0; i < Len / 2; i++) {
        uint8_t Tmp = p_Data[i];
        p_Data[i] = p_Data[Len - 1 - i];
        p_Data[Len - 1 - i] = Tmp;
    }
}

/* Escape a string for safe embedding in JSON values.
 * Replaces " with \" and \ with \\\.  Output is null-terminated.
 * Returns number of characters written (excluding null), or -1 if buffer too small. */
static int32_t JsonEscapeString(const char* p_In, char* p_Out, size_t MaxLen)
{
    size_t j = 0;
    for (size_t i = 0; p_In[i] != '\0'; i++) {
        if (p_In[i] == '"' || p_In[i] == '\\') {
            if (j + 2 >= MaxLen) return -1;
            p_Out[j++] = '\\';
            p_Out[j++] = p_In[i];
        } else if ((uint8_t)p_In[i] < 0x20) {
            /* Skip control characters */
            continue;
        } else {
            if (j + 1 >= MaxLen) return -1;
            p_Out[j++] = p_In[i];
        }
    }
    p_Out[j] = '\0';
    return (int32_t)j;
}

static PdqError_t SendJson(const char* p_Json)
{
    if (s_Ctx.Socket < 0) return PdqErrorNotConnected;

    printf("[STRATUM] TX: %s\n", p_Json);

    size_t Len = strlen(p_Json);
    ssize_t Sent = send(s_Ctx.Socket, p_Json, Len, 0);
    if (Sent != (ssize_t)Len) return PdqErrorTimeout;

    Sent = send(s_Ctx.Socket, "\n", 1, 0);
    if (Sent != 1) return PdqErrorTimeout;

    return PdqOk;
}

static char* FindJsonString(const char* p_Json, const char* p_Key, char* p_Out, size_t MaxLen)
{
    char Pattern[64];
    snprintf(Pattern, sizeof(Pattern), "\"%s\"", p_Key);
    char* p_Start = strstr(p_Json, Pattern);
    if (p_Start == NULL) return NULL;
    
    p_Start = strchr(p_Start + strlen(Pattern), ':');
    if (p_Start == NULL) return NULL;
    
    while (*p_Start == ' ' || *p_Start == ':') p_Start++;
    
    if (*p_Start == '"') {
        p_Start++;
        char* p_End = strchr(p_Start, '"');
        if (p_End == NULL) return NULL;
        size_t Len = (size_t)(p_End - p_Start);
        if (Len >= MaxLen) Len = MaxLen - 1;
        memcpy(p_Out, p_Start, Len);
        p_Out[Len] = '\0';
        return p_Out;
    }
    return NULL;
}

static int32_t FindJsonInt(const char* p_Json, const char* p_Key)
{
    char Pattern[64];
    snprintf(Pattern, sizeof(Pattern), "\"%s\"", p_Key);
    char* p_Start = strstr(p_Json, Pattern);
    if (p_Start == NULL) return -1;
    
    p_Start = strchr(p_Start + strlen(Pattern), ':');
    if (p_Start == NULL) return -1;
    
    while (*p_Start == ' ' || *p_Start == ':') p_Start++;
    return atoi(p_Start);
}

static bool FindJsonBool(const char* p_Json, const char* p_Key)
{
    char Pattern[64];
    snprintf(Pattern, sizeof(Pattern), "\"%s\"", p_Key);
    char* p_Start = strstr(p_Json, Pattern);
    if (p_Start == NULL) return false;

    p_Start = strchr(p_Start + strlen(Pattern), ':');
    if (p_Start == NULL) return false;

    while (*p_Start == ' ' || *p_Start == ':') p_Start++;
    return (strncmp(p_Start, "true", 4) == 0);
}

static PdqError_t HandleSubscribeResult(const char* p_Json)
{
    char Extranonce1[17] = {0};

    char* p_Result = strstr(p_Json, "\"result\"");
    if (p_Result == NULL) return PdqErrorInvalidJob;

    char* p_Arr = strchr(p_Result, '[');
    if (p_Arr == NULL) return PdqErrorInvalidJob;

    p_Arr = strchr(p_Arr + 1, '[');
    if (p_Arr) {
        char* p_End = strchr(p_Arr, ']');
        if (p_End) {
            p_Arr = p_End + 1;
        }
    }

    char* p_Quote = strchr(p_Arr, '"');
    if (p_Quote == NULL) return PdqErrorInvalidJob;
    p_Quote++;
    char* p_EndQuote = strchr(p_Quote, '"');
    if (p_EndQuote == NULL) return PdqErrorInvalidJob;

    size_t Len = (size_t)(p_EndQuote - p_Quote);
    if (Len > 16) Len = 16;
    memcpy(Extranonce1, p_Quote, Len);

    int32_t ByteLen = HexToBytes(Extranonce1, s_Ctx.Extranonce1, PDQ_STRATUM_MAX_EXTRANONCE_LEN);
    if (ByteLen < 0) return PdqErrorInvalidJob;
    s_Ctx.Extranonce1Len = (uint8_t)ByteLen;

    p_Quote = strchr(p_EndQuote + 1, ',');
    if (p_Quote) {
        while (*p_Quote == ',' || *p_Quote == ' ') p_Quote++;
        s_Ctx.Extranonce2Size = (uint32_t)atoi(p_Quote);
        if (s_Ctx.Extranonce2Size > PDQ_STRATUM_MAX_EXTRANONCE_LEN) {
            s_Ctx.Extranonce2Size = PDQ_STRATUM_MAX_EXTRANONCE_LEN;
        }
    }

    s_Ctx.State = StratumStateSubscribed;
    printf("[STRATUM] Extranonce1(%d bytes): ", s_Ctx.Extranonce1Len);
    for (int i = 0; i < s_Ctx.Extranonce1Len; i++) printf("%02x", s_Ctx.Extranonce1[i]);
    printf(" | Extranonce2Size: %u\n", (unsigned)s_Ctx.Extranonce2Size);
    return PdqOk;
}

static PdqError_t HandleAuthorizeResult(const char* p_Json)
{
    if (strstr(p_Json, "\"result\":true") || strstr(p_Json, "\"result\": true")) {
        s_Ctx.State = StratumStateAuthorized;
        return PdqOk;
    }
    return PdqErrorAuthFailed;
}

static PdqError_t HandleSetDifficulty(const char* p_Json)
{
    char* p_Params = strstr(p_Json, "\"params\"");
    if (p_Params == NULL) return PdqErrorInvalidJob;

    char* p_Arr = strchr(p_Params, '[');
    if (p_Arr == NULL) return PdqErrorInvalidJob;

    double Diff = strtod(p_Arr + 1, NULL);
    printf("[STRATUM] Pool requested difficulty: %f\n", Diff);
    /* Enforce minimum difficulty of 1.0 to avoid "Difficulty too low" rejections.
     * Some pools (e.g. public-pool.io) send set_difficulty below their actual
     * acceptance threshold. */
    if (Diff < 1.0) Diff = 1.0;
    s_Ctx.Difficulty = Diff > 0.0 ? Diff : 1.0;
    printf("[STRATUM] Using difficulty: %f\n", s_Ctx.Difficulty);
    return PdqOk;
}

static PdqError_t HandleNotify(const char* p_Json)
{
    char* p_Params = strstr(p_Json, "\"params\"");
    if (p_Params == NULL) return PdqErrorInvalidJob;

    char* p_Arr = strchr(p_Params, '[');
    if (p_Arr == NULL) return PdqErrorInvalidJob;

    char Buffer[256];
    char* p = p_Arr + 1;
    int Field = 0;

    static PdqStratumJob_t Job;
    memset(&Job, 0, sizeof(Job));

    while (*p && Field < 9) {
        while (*p == ' ' || *p == ',') p++;

        if (*p == '"') {
            p++;
            char* p_End = strchr(p, '"');
            if (p_End == NULL) break;
            size_t Len = (size_t)(p_End - p);
            if (Len >= sizeof(Buffer)) Len = sizeof(Buffer) - 1;
            memcpy(Buffer, p, Len);
            Buffer[Len] = '\0';

            switch (Field) {
                case 0:
                    strncpy(Job.JobId, Buffer, PDQ_STRATUM_MAX_JOBID_LEN);
                    Job.JobId[PDQ_STRATUM_MAX_JOBID_LEN] = '\0';
                    break;
                case 1:
                    HexToBytes(Buffer, Job.PrevBlockHash, 32);
                    /* Stratum prevhash: each 4-byte word is byte-reversed (LE).
                     * Swap bytes within each word to get internal byte order
                     * for the block header. (NOT a full 32-byte reverse.) */
                    for (int w = 0; w < 8; w++) {
                        uint32_t* pw = (uint32_t*)(Job.PrevBlockHash + w * 4);
                        *pw = __builtin_bswap32(*pw);
                    }
                    break;
                case 2: {
                    int32_t Len = HexToBytes(Buffer, Job.Coinbase1, PDQ_STRATUM_MAX_COINBASE_LEN);
                    Job.Coinbase1Len = (Len > 0) ? (uint16_t)Len : 0;
                    break;
                }
                case 3: {
                    int32_t Len = HexToBytes(Buffer, Job.Coinbase2, PDQ_STRATUM_MAX_COINBASE_LEN);
                    Job.Coinbase2Len = (Len > 0) ? (uint16_t)Len : 0;
                    break;
                }
                case 5:
                    Job.Version = (uint32_t)strtoul(Buffer, NULL, 16);
                    break;
                case 6:
                    Job.NBits = (uint32_t)strtoul(Buffer, NULL, 16);
                    break;
                case 7:
                    Job.NTime = (uint32_t)strtoul(Buffer, NULL, 16);
                    break;
            }
            p = p_End + 1;
            Field++;
        } else if (*p == '[') {
            if (Field == 4) {
                p++;
                Job.MerkleBranchCount = 0;
                while (*p && *p != ']' && Job.MerkleBranchCount < PDQ_STRATUM_MAX_MERKLE_BRANCHES) {
                    while (*p == ' ' || *p == ',') p++;
                    if (*p == '"') {
                        p++;
                        char* p_End = strchr(p, '"');
                        if (p_End == NULL) break;
                        size_t Len = (size_t)(p_End - p);
                        if (Len == 64) {
                            memcpy(Buffer, p, 64);
                            Buffer[64] = '\0';
                            HexToBytes(Buffer, Job.MerkleBranches[Job.MerkleBranchCount], 32);
                            Job.MerkleBranchCount++;
                        }
                        p = p_End + 1;
                    } else {
                        p++;
                    }
                }
                if (*p == ']') p++;
                Field++;
            } else {
                p++;
            }
        } else if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0) {
            if (Field == 8) {
                Job.CleanJobs = (strncmp(p, "true", 4) == 0);
            }
            p += (strncmp(p, "true", 4) == 0) ? 4 : 5;
            Field++;
        } else {
            p++;
        }
    }

    memcpy(&s_Ctx.CurrentJob, &Job, sizeof(Job));
    s_Ctx.HasNewJob = true;
    if (s_Ctx.State == StratumStateAuthorized) {
        s_Ctx.State = StratumStateReady;
    }

    return PdqOk;
}

static PdqError_t ProcessLine(const char* p_Line)
{
    printf("[STRATUM] RX: %s\n", p_Line);
    if (strstr(p_Line, "\"method\"")) {
        if (strstr(p_Line, "mining.set_difficulty")) {
            printf("[STRATUM] Got set_difficulty\n");
            return HandleSetDifficulty(p_Line);
        } else if (strstr(p_Line, "mining.notify")) {
            printf("[STRATUM] Got mining.notify!\n");
            return HandleNotify(p_Line);
        }
    } else if (strstr(p_Line, "\"id\"")) {
        int Id = FindJsonInt(p_Line, "id");
        if (Id == JSON_ID_SUBSCRIBE) {
            printf("[STRATUM] Got subscribe result\n");
            return HandleSubscribeResult(p_Line);
        } else if (Id == JSON_ID_AUTHORIZE) {
            printf("[STRATUM] Got authorize result\n");
            return HandleAuthorizeResult(p_Line);
        }
    }
    return PdqOk;
}

// Public API functions for stratum client

PdqError_t PdqStratumInit(void)
{
    memset(&s_Ctx, 0, sizeof(s_Ctx));
    s_Ctx.Socket = -1;
    s_Ctx.State = StratumStateDisconnected;
    s_Ctx.Difficulty = 1.0;
    return PdqOk;
}

PdqError_t PdqStratumConnect(const char* p_Host, uint16_t Port)
{
    if (p_Host == NULL || Port == 0) return PdqErrorInvalidParam;
    if (s_Ctx.Socket >= 0) PdqStratumDisconnect();

    s_Ctx.State = StratumStateConnecting;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char PortStr[8];
    snprintf(PortStr, sizeof(PortStr), "%d", Port);

    int err = getaddrinfo(p_Host, PortStr, &hints, &res);
    if (err != 0 || res == NULL) {
        s_Ctx.State = StratumStateDisconnected;
        return PdqErrorNotConnected;
    }

    s_Ctx.Socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s_Ctx.Socket < 0) {
        freeaddrinfo(res);
        s_Ctx.State = StratumStateDisconnected;
        return PdqErrorNotConnected;
    }

    struct timeval Timeout;
    Timeout.tv_sec = PDQ_STRATUM_DEFAULT_TIMEOUT_MS / 1000;
    Timeout.tv_usec = (PDQ_STRATUM_DEFAULT_TIMEOUT_MS % 1000) * 1000;
    setsockopt(s_Ctx.Socket, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof(Timeout));
    setsockopt(s_Ctx.Socket, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof(Timeout));

    if (connect(s_Ctx.Socket, res->ai_addr, res->ai_addrlen) < 0) {
        close(s_Ctx.Socket);
        s_Ctx.Socket = -1;
        freeaddrinfo(res);
        s_Ctx.State = StratumStateDisconnected;
        return PdqErrorNotConnected;
    }

    freeaddrinfo(res);
    s_Ctx.State = StratumStateConnected;
    return PdqOk;
}

PdqError_t PdqStratumDisconnect(void)
{
    if (s_Ctx.Socket >= 0) {
        close(s_Ctx.Socket);
        s_Ctx.Socket = -1;
    }
    s_Ctx.State = StratumStateDisconnected;
    s_Ctx.HasNewJob = false;
    return PdqOk;
}

PdqError_t PdqStratumSubscribe(void)
{
    if (s_Ctx.State != StratumStateConnected) return PdqErrorNotConnected;

    s_Ctx.State = StratumStateSubscribing;
    snprintf(s_Ctx.SendBuffer, sizeof(s_Ctx.SendBuffer),
             "{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[\"PDQminer/%d.%d.%d\"]}",
             JSON_ID_SUBSCRIBE, PDQ_VERSION_MAJOR, PDQ_VERSION_MINOR, PDQ_VERSION_PATCH);

    return SendJson(s_Ctx.SendBuffer);
}

PdqError_t PdqStratumSuggestDifficulty(double Difficulty)
{
    if (s_Ctx.Socket < 0) return PdqErrorNotConnected;

    snprintf(s_Ctx.SendBuffer, sizeof(s_Ctx.SendBuffer),
             "{\"id\":%d,\"method\":\"mining.suggest_difficulty\",\"params\":[%g]}",
             JSON_ID_SUGGEST_DIFF, Difficulty);

    return SendJson(s_Ctx.SendBuffer);
}

PdqError_t PdqStratumAuthorize(const char* p_Worker, const char* p_Password)
{
    if (s_Ctx.State != StratumStateSubscribed) return PdqErrorNotConnected;
    if (p_Worker == NULL) return PdqErrorInvalidParam;

    strncpy(s_Ctx.Worker, p_Worker, PDQ_MAX_WORKER_LEN);
    s_Ctx.Worker[PDQ_MAX_WORKER_LEN] = '\0';
    strncpy(s_Ctx.Password, p_Password ? p_Password : "x", PDQ_MAX_PASSWORD_LEN);
    s_Ctx.Password[PDQ_MAX_PASSWORD_LEN] = '\0';

    /* Escape worker and password for safe JSON embedding */
    char EscWorker[PDQ_MAX_WORKER_LEN * 2 + 1];
    char EscPassword[PDQ_MAX_PASSWORD_LEN * 2 + 1];
    JsonEscapeString(s_Ctx.Worker, EscWorker, sizeof(EscWorker));
    JsonEscapeString(s_Ctx.Password, EscPassword, sizeof(EscPassword));

    s_Ctx.State = StratumStateAuthorizing;
    snprintf(s_Ctx.SendBuffer, sizeof(s_Ctx.SendBuffer),
             "{\"id\":%d,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}",
             JSON_ID_AUTHORIZE, EscWorker, EscPassword);

    return SendJson(s_Ctx.SendBuffer);
}

PdqError_t PdqStratumSubmitShare(const char* p_JobId, uint32_t Extranonce2, uint32_t Nonce, uint32_t NTime)
{
    if (s_Ctx.State != StratumStateReady) return PdqErrorNotConnected;
    if (p_JobId == NULL) return PdqErrorInvalidParam;

    char Extranonce2Hex[17] = {0};
    uint8_t Extranonce2Bytes[8];
    uint32_t En2Size = s_Ctx.Extranonce2Size;
    if (En2Size > PDQ_STRATUM_MAX_EXTRANONCE_LEN) En2Size = PDQ_STRATUM_MAX_EXTRANONCE_LEN;
    for (int i = 0; i < (int)En2Size; i++) {
        Extranonce2Bytes[i] = (uint8_t)(Extranonce2 >> (i * 8));
    }
    BytesToHex(Extranonce2Bytes, En2Size, Extranonce2Hex);

    char NTimeHex[9] = {0};
    snprintf(NTimeHex, sizeof(NTimeHex), "%08x", NTime);

    char NonceHex[9] = {0};
    snprintf(NonceHex, sizeof(NonceHex), "%08x", Nonce);

    s_Ctx.SubmitId++;
    snprintf(s_Ctx.SendBuffer, sizeof(s_Ctx.SendBuffer),
             "{\"id\":%u,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}",
             JSON_ID_SUBMIT_BASE + s_Ctx.SubmitId, s_Ctx.Worker, p_JobId,
             Extranonce2Hex, NTimeHex, NonceHex);

    return SendJson(s_Ctx.SendBuffer);
}

PdqError_t PdqStratumProcess(void)
{
    if (s_Ctx.Socket < 0) return PdqErrorNotConnected;

    /* Guard against buffer-full condition: if buffer has no room for more data
     * and no complete line was found, discard the buffer to prevent deadlock. */
    if (s_Ctx.RecvLen >= PDQ_STRATUM_RECV_BUFFER_SIZE - 1) {
        printf("[STRATUM] WARN: recv buffer full (%u bytes), discarding\n", s_Ctx.RecvLen);
        s_Ctx.RecvLen = 0;
        s_Ctx.RecvBuffer[0] = '\0';
    }

    fd_set ReadSet;
    struct timeval Timeout = {0, 100000};

    FD_ZERO(&ReadSet);
    FD_SET(s_Ctx.Socket, &ReadSet);

    int Ready = select(s_Ctx.Socket + 1, &ReadSet, NULL, NULL, &Timeout);
    if (Ready <= 0) return PdqOk;

    ssize_t Bytes = recv(s_Ctx.Socket, s_Ctx.RecvBuffer + s_Ctx.RecvLen,
                         PDQ_STRATUM_RECV_BUFFER_SIZE - s_Ctx.RecvLen - 1, 0);

    if (Bytes <= 0) {
        PdqStratumDisconnect();
        return PdqErrorNotConnected;
    }

    s_Ctx.RecvLen += (uint16_t)Bytes;
    s_Ctx.RecvBuffer[s_Ctx.RecvLen] = '\0';

    char* p_Line = s_Ctx.RecvBuffer;
    char* p_Newline;

    while ((p_Newline = strchr(p_Line, '\n')) != NULL) {
        *p_Newline = '\0';
        ProcessLine(p_Line);
        p_Line = p_Newline + 1;
    }

    if (p_Line != s_Ctx.RecvBuffer) {
        size_t Remaining = strlen(p_Line);
        memmove(s_Ctx.RecvBuffer, p_Line, Remaining + 1);
        s_Ctx.RecvLen = (uint16_t)Remaining;
    }

    return PdqOk;
}

bool PdqStratumIsConnected(void)
{
    return s_Ctx.State >= StratumStateConnected;
}

bool PdqStratumIsReady(void)
{
    return s_Ctx.State == StratumStateReady;
}

bool PdqStratumHasNewJob(void)
{
    bool Has = s_Ctx.HasNewJob;
    s_Ctx.HasNewJob = false;
    return Has;
}

PdqStratumState_t PdqStratumGetState(void)
{
    return s_Ctx.State;
}

PdqError_t PdqStratumGetJob(PdqStratumJob_t* p_Job)
{
    if (p_Job == NULL) return PdqErrorInvalidParam;
    memcpy(p_Job, &s_Ctx.CurrentJob, sizeof(PdqStratumJob_t));
    return PdqOk;
}

double PdqStratumGetDifficulty(void)
{
    return s_Ctx.Difficulty;
}

void PdqStratumGetExtranonce(uint8_t* p_Buffer, uint8_t* p_Len)
{
    if (p_Buffer && p_Len) {
        memcpy(p_Buffer, s_Ctx.Extranonce1, s_Ctx.Extranonce1Len);
        *p_Len = s_Ctx.Extranonce1Len;
    }
}

uint8_t PdqStratumGetExtranonce2Size(void)
{
    return (uint8_t)s_Ctx.Extranonce2Size;
}

static void DifficultyToTarget(double Difficulty, uint32_t* p_Target)
{
    /* Bitcoin pool difficulty 1 target (pdiff) as LE uint256 (pn[] format):
     *   Number: 0x00000000_FFFF0000_00000000_00000000_00000000_00000000_00000000_00000000
     *   = 0xFFFF * 2^208
     *   pn[7]=0, pn[6]=0xFFFF0000, pn[5..0]=0  (pn[7] = most significant word)
     *
     * For arbitrary difficulty D: target = pdiff1 / D
     * We compute pn[7]:pn[6] from 0x00000000FFFF0000 / D
     * Hash comparison: pn[7] first (most significant), down to pn[0]. */
    if (Difficulty <= 0.0) Difficulty = 1.0;

    memset(p_Target, 0, 32);  /* All words zero */

    if (Difficulty < 1.0) {
        /* Fractional difficulty: target > pdiff1.
         * Compute full division; result may overflow into pn[7]. */
        double top64_f = (double)0x00000000FFFF0000ULL / Difficulty;
        if (top64_f >= (double)UINT64_MAX) {
            /* Overflow: accept (almost) everything */
            memset(p_Target, 0xFF, 32);
        } else {
            uint64_t top64 = (uint64_t)top64_f;
            p_Target[7] = (uint32_t)(top64 >> 32);
            p_Target[6] = (uint32_t)(top64 & 0xFFFFFFFF);
            /* Fill lower words with 0xFF for easy targets */
            memset(p_Target, 0xFF, 24);  /* p_Target[0..5] = 0xFFFFFFFF */
        }
    } else {
        /* Integer difficulty >= 1: standard case */
        uint64_t top64 = 0x00000000FFFF0000ULL / (uint64_t)Difficulty;
        p_Target[7] = (uint32_t)(top64 >> 32);
        p_Target[6] = (uint32_t)(top64 & 0xFFFFFFFF);
        /* p_Target[5..0] remain 0 from memset */
    }
}

PdqError_t PdqStratumBuildMiningJob(const PdqStratumJob_t* p_StratumJob,
                                     const uint8_t* p_Extranonce1, uint8_t Extranonce1Len,
                                     uint32_t Extranonce2, uint8_t Extranonce2Len,
                                     double Difficulty,
                                     PdqMiningJob_t* p_MiningJob)
{
    if (p_StratumJob == NULL || p_MiningJob == NULL) return PdqErrorInvalidParam;

    memset(p_MiningJob, 0, sizeof(PdqMiningJob_t));

    uint8_t Coinbase[600];
    size_t CoinbaseLen = 0;

    /* Bounds-check total coinbase length before assembly.
     * Max: Coinbase1(256) + Extranonce1(8) + Extranonce2(8) + Coinbase2(256) = 528. */
    size_t TotalLen = (size_t)p_StratumJob->Coinbase1Len + Extranonce1Len +
                      Extranonce2Len + (size_t)p_StratumJob->Coinbase2Len;
    if (TotalLen > sizeof(Coinbase)) {
        printf("[STRATUM] ERROR: coinbase too large (%u bytes), max %u\n",
               (unsigned)TotalLen, (unsigned)sizeof(Coinbase));
        return PdqErrorInvalidJob;
    }

    memcpy(Coinbase + CoinbaseLen, p_StratumJob->Coinbase1, p_StratumJob->Coinbase1Len);
    CoinbaseLen += p_StratumJob->Coinbase1Len;

    if (p_Extranonce1 && Extranonce1Len > 0) {
        memcpy(Coinbase + CoinbaseLen, p_Extranonce1, Extranonce1Len);
        CoinbaseLen += Extranonce1Len;
    }

    /* Clamp Extranonce2Len to prevent overflow */
    if (Extranonce2Len > PDQ_STRATUM_MAX_EXTRANONCE_LEN) Extranonce2Len = PDQ_STRATUM_MAX_EXTRANONCE_LEN;
    for (int i = 0; i < Extranonce2Len; i++) {
        Coinbase[CoinbaseLen++] = (uint8_t)(Extranonce2 >> (i * 8));
    }

    memcpy(Coinbase + CoinbaseLen, p_StratumJob->Coinbase2, p_StratumJob->Coinbase2Len);
    CoinbaseLen += p_StratumJob->Coinbase2Len;

    uint8_t MerkleRoot[32];
    PdqSha256d(Coinbase, CoinbaseLen, MerkleRoot);

    printf("[DBG] CoinbaseHash: ");
    for (int i = 0; i < 32; i++) printf("%02x", MerkleRoot[i]);
    printf("\n");

    for (uint8_t i = 0; i < p_StratumJob->MerkleBranchCount; i++) {
        uint8_t Concat[64];
        memcpy(Concat, MerkleRoot, 32);
        memcpy(Concat + 32, p_StratumJob->MerkleBranches[i], 32);
        PdqSha256d(Concat, 64, MerkleRoot);
        if (i < 2) {
            printf("[DBG] Branch[%d]: ", i);
            for (int j = 0; j < 32; j++) printf("%02x", p_StratumJob->MerkleBranches[i][j]);
            printf(" -> MR: ");
            for (int j = 0; j < 32; j++) printf("%02x", MerkleRoot[j]);
            printf("\n");
        }
    }

    uint8_t Header[80];
    Header[0] = (uint8_t)(p_StratumJob->Version);
    Header[1] = (uint8_t)(p_StratumJob->Version >> 8);
    Header[2] = (uint8_t)(p_StratumJob->Version >> 16);
    Header[3] = (uint8_t)(p_StratumJob->Version >> 24);

    memcpy(Header + 4, p_StratumJob->PrevBlockHash, 32);
    memcpy(Header + 36, MerkleRoot, 32);

    Header[68] = (uint8_t)(p_StratumJob->NTime);
    Header[69] = (uint8_t)(p_StratumJob->NTime >> 8);
    Header[70] = (uint8_t)(p_StratumJob->NTime >> 16);
    Header[71] = (uint8_t)(p_StratumJob->NTime >> 24);

    Header[72] = (uint8_t)(p_StratumJob->NBits);
    Header[73] = (uint8_t)(p_StratumJob->NBits >> 8);
    Header[74] = (uint8_t)(p_StratumJob->NBits >> 16);
    Header[75] = (uint8_t)(p_StratumJob->NBits >> 24);

    memset(Header + 76, 0, 4);

    /* Debug: print extranonce1, merkle root, coinbase, and full header */
    printf("[DBG] EN1(%d): ", Extranonce1Len);
    for (int i = 0; i < Extranonce1Len; i++) printf("%02x", p_Extranonce1[i]);
    printf("\n");
    printf("[DBG] EN2(%d): ", Extranonce2Len);
    for (int i = 0; i < Extranonce2Len; i++) printf("%02x", (uint8_t)(Extranonce2 >> (i * 8)));
    printf("\n");
    printf("[DBG] Coinbase(%d): ", (int)CoinbaseLen);
    for (size_t i = 0; i < CoinbaseLen; i++) printf("%02x", Coinbase[i]);
    printf("\n");
    printf("[DBG] MerkleRoot: ");
    for (int i = 0; i < 32; i++) printf("%02x", MerkleRoot[i]);
    printf("\n");
    printf("[DBG] Header(80B): ");
    for (int i = 0; i < 80; i++) printf("%02x", Header[i]);
    printf("\n");

    PdqSha256Midstate(Header, p_MiningJob->Midstate);

    memcpy(p_MiningJob->BlockTail, Header + 64, 16);
    p_MiningJob->BlockTail[16] = 0x80;
    memset(p_MiningJob->BlockTail + 17, 0, 45);
    p_MiningJob->BlockTail[62] = 0x02;
    p_MiningJob->BlockTail[63] = 0x80;

    /* Prepare byte-swapped header + padding for HW SHA engine (ESP32-D0).
     * The HW SHA peripheral expects big-endian words written to registers. */
    for (int i = 0; i < 20; i++) {
        p_MiningJob->HeaderSwapped[i] = __builtin_bswap32(((const uint32_t*)Header)[i]);
    }
    /* SHA padding for 80 bytes: 0x80 byte, zeros, then length = 640 bits = 0x280 */
    p_MiningJob->HeaderSwapped[20] = 0x80000000;
    for (int i = 21; i < 31; i++) {
        p_MiningJob->HeaderSwapped[i] = 0;
    }
    p_MiningJob->HeaderSwapped[31] = 0x00000280;

    DifficultyToTarget(Difficulty, p_MiningJob->Target);

    strncpy(p_MiningJob->JobId, p_StratumJob->JobId, 64);
    p_MiningJob->JobId[64] = '\0';
    p_MiningJob->Extranonce2 = Extranonce2;
    p_MiningJob->NTime = p_StratumJob->NTime;

    return PdqOk;
}