/**
 * @file display_driver.cpp
 * @brief Minimal terminal-style TFT display driver
 *
 * Renders a simple monochrome terminal display. Static info (pool, IP, worker)
 * is drawn once at boot before mining starts (no APB bus contention). Dynamic
 * stats (hashrate, shares) are updated via tiny padded drawString calls with
 * mining briefly paused (~5ms every 10s, negligible throughput impact).
 *
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "display_driver.h"

#ifndef PDQ_HEADLESS

#include <TFT_eSPI.h>
#include <Arduino.h>
#include <stdio.h>

#define BG       0x0000   /* black */
#define FG       0x07E0   /* green - terminal style */
#define FG_DIM   0x03E0   /* dark green for labels */
#define FG_CYAN  0x07FF   /* cyan for hashrate */
#define SCREEN_W 320
#define SCREEN_H 240

/* Y positions for each line (Font 2 = 16px height) */
#define LINE_H   20
#define Y_TITLE   4
#define Y_SEP1   (Y_TITLE + LINE_H + 2)
#define Y_POOL   (Y_SEP1 + 6)
#define Y_IP     (Y_POOL + LINE_H)
#define Y_WORKER (Y_IP + LINE_H)
#define Y_SEP2   (Y_WORKER + LINE_H + 4)
#define Y_HASH   (Y_SEP2 + 8)
#define Y_DETAIL (Y_HASH + LINE_H + 2)
#define Y_SEP3   (Y_DETAIL + LINE_H + 4)
#define Y_SHARES (Y_SEP3 + 8)
#define Y_BLOCKS (Y_SHARES + LINE_H)
#define Y_DIFF   (Y_BLOCKS + LINE_H)

static TFT_eSPI s_Tft = TFT_eSPI();
static PdqDisplayMode_t s_Mode = PdqDisplayModeMinimal;

extern "C" {

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode)
{
    s_Mode = Mode;
    if (Mode == PdqDisplayModeHeadless) return PdqOk;

    s_Tft.init();
    s_Tft.setRotation(3);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    s_Tft.fillScreen(BG);
    s_Tft.setTextSize(1);

    /* Title bar */
    s_Tft.setTextColor(FG_CYAN, BG);
    s_Tft.setTextDatum(TL_DATUM);
    char Title[40];
    snprintf(Title, sizeof(Title), "PDQminer v%d.%d.%d",
             PDQ_VERSION_MAJOR, PDQ_VERSION_MINOR, PDQ_VERSION_PATCH);
    s_Tft.drawString(Title, 4, Y_TITLE, 2);

    /* Separator */
    s_Tft.drawFastHLine(4, Y_SEP1, SCREEN_W - 8, FG_DIM);

    return PdqOk;
}

PdqError_t PdqDisplayShowInfo(const char* p_Pool, const char* p_Ip, const char* p_Worker)
{
    if (s_Mode == PdqDisplayModeHeadless) return PdqOk;

    s_Tft.setTextSize(1);
    s_Tft.setTextDatum(TL_DATUM);

    /* Pool */
    s_Tft.setTextColor(FG_DIM, BG);
    s_Tft.drawString("Pool:", 4, Y_POOL, 2);
    s_Tft.setTextColor(FG, BG);
    if (p_Pool) s_Tft.drawString(p_Pool, 56, Y_POOL, 2);

    /* IP */
    s_Tft.setTextColor(FG_DIM, BG);
    s_Tft.drawString("IP:", 4, Y_IP, 2);
    s_Tft.setTextColor(FG, BG);
    if (p_Ip) s_Tft.drawString(p_Ip, 56, Y_IP, 2);

    /* Worker */
    s_Tft.setTextColor(FG_DIM, BG);
    s_Tft.drawString("Node:", 4, Y_WORKER, 2);
    s_Tft.setTextColor(FG, BG);
    if (p_Worker) s_Tft.drawString(p_Worker, 56, Y_WORKER, 2);

    /* Separator */
    s_Tft.drawFastHLine(4, Y_SEP2, SCREEN_W - 8, FG_DIM);

    /* Static labels for dynamic section */
    s_Tft.setTextColor(FG_DIM, BG);
    s_Tft.drawString("Hashrate:", 4, Y_HASH, 2);
    s_Tft.drawString("HW/SW:", 4, Y_DETAIL, 2);
    s_Tft.drawFastHLine(4, Y_SEP3, SCREEN_W - 8, FG_DIM);
    s_Tft.drawString("Shares:", 4, Y_SHARES, 2);
    s_Tft.drawString("Blocks:", 4, Y_BLOCKS, 2);
    s_Tft.drawString("Diff:", 4, Y_DIFF, 2);

    return PdqOk;
}

PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats)
{
    if (p_Stats == NULL) return PdqErrorInvalidParam;
    if (s_Mode == PdqDisplayModeHeadless) return PdqOk;

    char Buf[40];
    s_Tft.setTextSize(1);
    s_Tft.setTextDatum(TL_DATUM);
    s_Tft.setTextPadding(200);

    /* Hashrate */
    snprintf(Buf, sizeof(Buf), "%lu KH/s", (unsigned long)(p_Stats->HashRate / 1000));
    s_Tft.setTextColor(FG_CYAN, BG);
    s_Tft.drawString(Buf, 110, Y_HASH, 2);

    /* HW/SW breakdown */
    snprintf(Buf, sizeof(Buf), "%lu / %lu KH/s",
             (unsigned long)(p_Stats->HashRateHw / 1000),
             (unsigned long)(p_Stats->HashRateSw / 1000));
    s_Tft.setTextColor(FG, BG);
    s_Tft.drawString(Buf, 70, Y_DETAIL, 2);

    /* Shares */
    snprintf(Buf, sizeof(Buf), "%lu accepted  %lu rejected",
             (unsigned long)p_Stats->SharesAccepted,
             (unsigned long)p_Stats->SharesRejected);
    s_Tft.drawString(Buf, 80, Y_SHARES, 2);

    /* Blocks */
    snprintf(Buf, sizeof(Buf), "%lu", (unsigned long)p_Stats->BlocksFound);
    s_Tft.drawString(Buf, 80, Y_BLOCKS, 2);

    /* Difficulty */
    snprintf(Buf, sizeof(Buf), "%.3f", p_Stats->Difficulty);
    s_Tft.drawString(Buf, 80, Y_DIFF, 2);

    s_Tft.setTextPadding(0);
    return PdqOk;
}

PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2)
{
    if (s_Mode == PdqDisplayModeHeadless) return PdqOk;

    s_Tft.setTextColor(FG, BG);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.setTextSize(1);
    if (p_Line1) s_Tft.drawString(p_Line1, SCREEN_W / 2, 100, 2);
    if (p_Line2) s_Tft.drawString(p_Line2, SCREEN_W / 2, 125, 2);
    s_Tft.setTextDatum(TL_DATUM);
    return PdqOk;
}

PdqError_t PdqDisplaySetBrightness(uint8_t Percent)
{
    if (s_Mode == PdqDisplayModeHeadless) return PdqOk;
#ifdef TFT_BL
    uint8_t Value = (Percent > 100) ? 255 : (Percent * 255 / 100);
    analogWrite(TFT_BL, Value);
#endif
    return PdqOk;
}

PdqError_t PdqDisplayOff(void)
{
    if (s_Mode == PdqDisplayModeHeadless) return PdqOk;
#ifdef TFT_BL
    digitalWrite(TFT_BL, LOW);
#endif
    return PdqOk;
}

PdqDisplayMode_t PdqDisplayCycleTheme(void)
{
    return s_Mode;
}

} /* extern "C" */

#else /* PDQ_HEADLESS */

extern "C" {

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode)
{
    (void)Mode;
    return PdqOk;
}

PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats)
{
    (void)p_Stats;
    return PdqOk;
}

PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2)
{
    (void)p_Line1;
    (void)p_Line2;
    return PdqOk;
}

PdqError_t PdqDisplayShowInfo(const char* p_Pool, const char* p_Ip, const char* p_Worker)
{
    (void)p_Pool;
    (void)p_Ip;
    (void)p_Worker;
    return PdqOk;
}

PdqError_t PdqDisplaySetBrightness(uint8_t Percent)
{
    (void)Percent;
    return PdqOk;
}

PdqError_t PdqDisplayOff(void)
{
    return PdqOk;
}

PdqDisplayMode_t PdqDisplayCycleTheme(void)
{
    return PdqDisplayModeHeadless;
}

} /* extern "C" */

#endif /* PDQ_HEADLESS */
