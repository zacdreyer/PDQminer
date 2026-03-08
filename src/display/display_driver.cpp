/**
 * @file display_driver.cpp
 * @brief Display driver with Trinity Pro and Trinity Lite themes
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "display_driver.h"

#ifndef PDQ_HEADLESS

#include <TFT_eSPI.h>
#include <Arduino.h>

/* ── Trinity Pro palette ─────────────────────────────────────────────── */
#define PRO_BG           0x0841   /* #0B0F14 */
#define PRO_PANEL        0x08C4   /* #111826 */
#define PRO_ACCENT       0x0737   /* #00E5FF — electric cyan */
#define PRO_TEXT         0xFFFF   /* white */
#define PRO_TEXT_DIM     0x9CF3   /* #9AA6B2 — secondary text */
#define PRO_ALERT        0x07F2   /* #00FFC6 */

/* ── Trinity Lite palette ────────────────────────────────────────────── */
#define LITE_BG          0x0020   /* #05070A */
#define LITE_ACCENT      0x06BF   /* #00D4FF — soft cyan */
#define LITE_TEXT        0xEF7E   /* #EAF2F8 */
#define LITE_TEXT_DIM    0x7CB3   /* #7A8A99 */

/* ── Common ──────────────────────────────────────────────────────────── */
#define SCREEN_W         320
#define SCREEN_H         240

static TFT_eSPI s_Tft = TFT_eSPI();
static PdqDisplayMode_t s_Mode = PdqDisplayModeTrinityPro;
static uint32_t s_LastUpdate = 0;
static bool s_NeedsFullRedraw = true;

/* ── Forward declarations ────────────────────────────────────────────── */
static void DrawTrinityIcon(int16_t Cx, int16_t Cy, int16_t Scale, uint16_t Color);
static void FormatHashRate(uint32_t HashRate, char* p_Buf, size_t Len, char* p_Unit, size_t UnitLen);
static void FormatUptime(uint32_t Seconds, char* p_Buf, size_t Len);
static void FormatTotalHashes(uint64_t Total, char* p_Buf, size_t Len);
static void FormatDifficulty(double Diff, char* p_Buf, size_t Len);

/* WiFi icon (12x9 drawn with arcs) */
static void DrawWifiIcon(int16_t X, int16_t Y, uint16_t Color, bool Connected);
/* Chip icon (abstract microchip) */
static void DrawChipIcon(int16_t X, int16_t Y, uint16_t Color);
/* Temperature with color coding */
static uint16_t TempColor(float Temp, uint16_t Normal, uint16_t Warn, uint16_t Alert);

/* ── Pro theme rendering ─────────────────────────────────────────────── */
static void ProDrawFrame(void);
static void ProDrawStats(const PdqMinerStats_t* p_Stats);

/* ── Lite theme rendering ────────────────────────────────────────────── */
static void LiteDrawFrame(void);
static void LiteDrawStats(const PdqMinerStats_t* p_Stats);

extern "C" {

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode)
{
    s_Mode = Mode;

    if (Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }

    s_Tft.init();
    s_Tft.setRotation(3);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    s_NeedsFullRedraw = true;

    if (Mode == PdqDisplayModeTrinityLite) {
        s_Tft.fillScreen(LITE_BG);
        LiteDrawFrame();
    } else if (Mode == PdqDisplayModeTrinityPro) {
        s_Tft.fillScreen(PRO_BG);
        ProDrawFrame();
    } else {
        /* Legacy minimal/standard modes — simple dark background */
        s_Tft.fillScreen(TFT_BLACK);
    }

    return PdqOk;
}

PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats)
{
    if (p_Stats == NULL) {
        return PdqErrorInvalidParam;
    }

    if (s_Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }

    uint32_t Now = millis();
    if (Now - s_LastUpdate < 1000) {
        return PdqOk;
    }
    s_LastUpdate = Now;

    if (s_Mode == PdqDisplayModeTrinityPro) {
        ProDrawStats(p_Stats);
    } else if (s_Mode == PdqDisplayModeTrinityLite) {
        LiteDrawStats(p_Stats);
    } else {
        /* Legacy fallback */
        ProDrawStats(p_Stats);
    }

    s_NeedsFullRedraw = false;
    return PdqOk;
}

PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2)
{
    if (s_Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }

    uint16_t Bg = (s_Mode == PdqDisplayModeTrinityLite) ? LITE_BG : PRO_BG;
    uint16_t Fg = (s_Mode == PdqDisplayModeTrinityLite) ? LITE_TEXT : PRO_TEXT;

    s_Tft.fillRect(0, 80, SCREEN_W, 80, Bg);
    s_Tft.setTextColor(Fg, Bg);
    s_Tft.setTextSize(2);

    if (p_Line1 != NULL) {
        s_Tft.setTextDatum(TC_DATUM);
        s_Tft.drawString(p_Line1, SCREEN_W / 2, 90, 2);
    }

    if (p_Line2 != NULL) {
        s_Tft.setTextDatum(TC_DATUM);
        s_Tft.drawString(p_Line2, SCREEN_W / 2, 120, 2);
    }

    s_Tft.setTextDatum(TL_DATUM);
    return PdqOk;
}

PdqError_t PdqDisplaySetBrightness(uint8_t Percent)
{
    if (s_Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }

#ifdef TFT_BL
    uint8_t Value = (Percent > 100) ? 255 : (Percent * 255 / 100);
    analogWrite(TFT_BL, Value);
#endif

    return PdqOk;
}

PdqError_t PdqDisplayOff(void)
{
    if (s_Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }

#ifdef TFT_BL
    digitalWrite(TFT_BL, LOW);
#endif

    return PdqOk;
}

PdqDisplayMode_t PdqDisplayCycleTheme(void)
{
    if (s_Mode == PdqDisplayModeTrinityPro) {
        s_Mode = PdqDisplayModeTrinityLite;
        s_Tft.fillScreen(LITE_BG);
        LiteDrawFrame();
    } else {
        s_Mode = PdqDisplayModeTrinityPro;
        s_Tft.fillScreen(PRO_BG);
        ProDrawFrame();
    }
    s_NeedsFullRedraw = true;
    return s_Mode;
}

} /* extern "C" */

/* ═══════════════════════════════════════════════════════════════════════
 *  TRINITY ICON — Three figures holding hands
 *  Cx,Cy = center of the icon group
 *  Scale  = 1 (tiny ~20px wide) to 4 (large ~80px wide)
 * ═══════════════════════════════════════════════════════════════════════ */
static void DrawTrinityIcon(int16_t Cx, int16_t Cy, int16_t Scale, uint16_t Color)
{
    /* Figure spacing */
    int16_t Sp = 8 * Scale;             /* horizontal spacing between figure centers */
    int16_t HeadR = 2 * Scale;          /* head radius */
    int16_t BodyH = 6 * Scale;          /* body height */
    int16_t LegH  = 4 * Scale;          /* leg height */
    int16_t ArmW  = 3 * Scale;          /* arm reach outward */
    int16_t ShortAdj = 2 * Scale;       /* child is shorter */

    /* Three figure center X positions */
    int16_t Fx[3] = { (int16_t)(Cx - Sp), Cx, (int16_t)(Cx + Sp) };
    /* Heights: left=tall, center=short(child), right=tall */
    int16_t HeadY[3]  = { Cy, (int16_t)(Cy + ShortAdj), Cy };
    int16_t BodyTop[3], BodyBot[3], LegBot[3];

    for (int i = 0; i < 3; i++) {
        BodyTop[i] = HeadY[i] + HeadR + 1;
        BodyBot[i] = BodyTop[i] + BodyH;
        LegBot[i]  = BodyBot[i] + LegH;
    }

    for (int i = 0; i < 3; i++) {
        /* Head (circle outline) */
        s_Tft.drawCircle(Fx[i], HeadY[i], HeadR, Color);

        /* Body (vertical line) */
        s_Tft.drawLine(Fx[i], BodyTop[i], Fx[i], BodyBot[i], Color);

        /* Legs (V shape) */
        int16_t LegSpread = 2 * Scale;
        s_Tft.drawLine(Fx[i], BodyBot[i], Fx[i] - LegSpread, LegBot[i], Color);
        s_Tft.drawLine(Fx[i], BodyBot[i], Fx[i] + LegSpread, LegBot[i], Color);
    }

    /* Arms connecting the figures (holding hands) */
    /* Arm Y is at about 40% down the body */
    int16_t ArmY0 = BodyTop[0] + BodyH * 4 / 10;
    int16_t ArmY1 = BodyTop[1] + (BodyH - ShortAdj) * 4 / 10 + ShortAdj;
    int16_t ArmY2 = BodyTop[2] + BodyH * 4 / 10;

    /* Left figure right arm → center figure left arm */
    s_Tft.drawLine(Fx[0] + ArmW, ArmY0, Fx[1] - ArmW, ArmY1, Color);
    /* Center figure right arm → right figure left arm */
    s_Tft.drawLine(Fx[1] + ArmW, ArmY1, Fx[2] - ArmW, ArmY2, Color);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  HELPER ICONS
 * ═══════════════════════════════════════════════════════════════════════ */
static void DrawWifiIcon(int16_t X, int16_t Y, uint16_t Color, bool Connected)
{
    uint16_t C = Connected ? Color : PRO_TEXT_DIM;
    /* Three arcs + dot */
    s_Tft.drawCircle(X, Y + 8, 2, C);
    s_Tft.drawLine(X - 5, Y + 4, X - 3, Y + 6, C);
    s_Tft.drawLine(X + 5, Y + 4, X + 3, Y + 6, C);
    s_Tft.drawLine(X - 8, Y, X - 5, Y + 3, C);
    s_Tft.drawLine(X + 8, Y, X + 5, Y + 3, C);
    s_Tft.fillCircle(X, Y + 8, 1, C);
}

static void DrawChipIcon(int16_t X, int16_t Y, uint16_t Color)
{
    /* Small 12x12 chip body */
    s_Tft.drawRect(X, Y, 12, 12, Color);
    s_Tft.drawRect(X + 3, Y + 3, 6, 6, Color);
    /* Pins */
    for (int i = 0; i < 3; i++) {
        int16_t Offset = 2 + i * 3;
        s_Tft.drawLine(X + Offset, Y - 2, X + Offset, Y, Color);
        s_Tft.drawLine(X + Offset, Y + 12, X + Offset, Y + 14, Color);
        s_Tft.drawLine(X - 2, Y + Offset, X, Y + Offset, Color);
        s_Tft.drawLine(X + 12, Y + Offset, X + 14, Y + Offset, Color);
    }
}

static uint16_t TempColor(float Temp, uint16_t Normal, uint16_t Warn, uint16_t Alert)
{
    if (Temp > 70.0f) return Alert;
    if (Temp > 55.0f) return Warn;
    return Normal;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  FORMAT HELPERS
 * ═══════════════════════════════════════════════════════════════════════ */
static void FormatHashRate(uint32_t HashRate, char* p_Buf, size_t Len, char* p_Unit, size_t UnitLen)
{
    if (HashRate >= 1000000) {
        snprintf(p_Buf, Len, "%.1f", HashRate / 1000000.0f);
        snprintf(p_Unit, UnitLen, "MH/s");
    } else if (HashRate >= 1000) {
        snprintf(p_Buf, Len, "%.1f", HashRate / 1000.0f);
        snprintf(p_Unit, UnitLen, "KH/s");
    } else {
        snprintf(p_Buf, Len, "%lu", (unsigned long)HashRate);
        snprintf(p_Unit, UnitLen, "H/s");
    }
}

static void FormatUptime(uint32_t Seconds, char* p_Buf, size_t Len)
{
    uint32_t Hours = Seconds / 3600;
    uint32_t Mins  = (Seconds % 3600) / 60;
    snprintf(p_Buf, Len, "%02lu:%02lu", (unsigned long)Hours, (unsigned long)Mins);
}

static void FormatTotalHashes(uint64_t Total, char* p_Buf, size_t Len)
{
    if (Total >= 1000000000ULL) {
        snprintf(p_Buf, Len, "%.1fG", Total / 1000000000.0);
    } else if (Total >= 1000000ULL) {
        snprintf(p_Buf, Len, "%.1fM", Total / 1000000.0);
    } else if (Total >= 1000ULL) {
        snprintf(p_Buf, Len, "%.1fK", Total / 1000.0);
    } else {
        snprintf(p_Buf, Len, "%llu", (unsigned long long)Total);
    }
}

static void FormatDifficulty(double Diff, char* p_Buf, size_t Len)
{
    if (Diff >= 1000000.0) {
        snprintf(p_Buf, Len, "%.1fM", Diff / 1000000.0);
    } else if (Diff >= 1000.0) {
        snprintf(p_Buf, Len, "%.1fK", Diff / 1000.0);
    } else if (Diff >= 1.0) {
        snprintf(p_Buf, Len, "%.1f", Diff);
    } else {
        snprintf(p_Buf, Len, "%.3f", Diff);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  TRINITY PRO THEME
 *
 *  Layout (320x240):
 *  ┌──────────────────────────────────────┐ 0
 *  │ [chip]   HH:MM   [wifi] XX°C        │ 28  ← status bar
 *  ├─────┬────────────────────────┬───────┤ 29  ← 1px accent line
 *  │     │                        │       │
 *  │icon │   742.6  KH/s         │Shares │ 30-160
 *  │     │   152.6M total        │Diff   │
 *  │NODE │                        │Tmpl   │
 *  │ACTV │                        │Best   │
 *  │     │                        │       │
 *  ├─────┴────────────────────────┴───────┤
 *  └──────────────────────────────────────┘ 240
 * ═══════════════════════════════════════════════════════════════════════ */

#define PRO_BAR_H        28
#define PRO_LEFT_W       70
#define PRO_RIGHT_W      90
#define PRO_CENTER_X     (PRO_LEFT_W)
#define PRO_CENTER_W     (SCREEN_W - PRO_LEFT_W - PRO_RIGHT_W)
#define PRO_RIGHT_X      (SCREEN_W - PRO_RIGHT_W)

static void ProDrawFrame(void)
{
    /* Status bar background */
    s_Tft.fillRect(0, 0, SCREEN_W, PRO_BAR_H, PRO_PANEL);

    /* 1px accent divider below status bar */
    s_Tft.drawFastHLine(0, PRO_BAR_H, SCREEN_W, PRO_ACCENT);

    /* Chip icon in status bar */
    DrawChipIcon(8, 7, PRO_ACCENT);

    /* Left identity panel - subtle panel background */
    s_Tft.fillRect(0, PRO_BAR_H + 1, PRO_LEFT_W, SCREEN_H - PRO_BAR_H - 1, PRO_PANEL);

    /* Trinity icon centered in left panel */
    DrawTrinityIcon(PRO_LEFT_W / 2, PRO_BAR_H + 55, 2, PRO_ACCENT);

    /* "NODE ACTIVE" label below icon */
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_PANEL);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.setTextSize(1);
    s_Tft.drawString("NODE", PRO_LEFT_W / 2, PRO_BAR_H + 100, 1);
    s_Tft.drawString("ACTIVE", PRO_LEFT_W / 2, PRO_BAR_H + 112, 1);
    s_Tft.setTextDatum(TL_DATUM);

    /* Right stats panel — thin cyan border */
    s_Tft.drawRect(PRO_RIGHT_X - 1, PRO_BAR_H + 1, PRO_RIGHT_W + 1, SCREEN_H - PRO_BAR_H - 1, PRO_ACCENT);

    /* Vertical separator on left panel right edge */
    s_Tft.drawFastVLine(PRO_LEFT_W - 1, PRO_BAR_H + 1, SCREEN_H - PRO_BAR_H - 1, PRO_ACCENT);
}

static void ProDrawStats(const PdqMinerStats_t* p_Stats)
{
    char Buf[32];
    char Unit[8];

    /* ── Status bar ── */
    /* Uptime center */
    FormatUptime(p_Stats->Uptime, Buf, sizeof(Buf));
    s_Tft.setTextColor(PRO_TEXT, PRO_PANEL);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.setTextSize(1);
    s_Tft.fillRect(100, 2, 120, 24, PRO_PANEL);
    s_Tft.drawString(Buf, SCREEN_W / 2, 9, 2);

    /* WiFi icon + temp on right side of status bar */
    s_Tft.fillRect(240, 0, 80, PRO_BAR_H, PRO_PANEL);
    DrawWifiIcon(255, 5, PRO_ACCENT, p_Stats->WifiConnected);

    /* Temperature */
    snprintf(Buf, sizeof(Buf), "%.0fC", p_Stats->Temperature);
    uint16_t Tc = TempColor(p_Stats->Temperature, PRO_TEXT, TFT_ORANGE, TFT_RED);
    s_Tft.setTextColor(Tc, PRO_PANEL);
    s_Tft.setTextDatum(TR_DATUM);
    s_Tft.drawString(Buf, SCREEN_W - 6, 9, 2);

    /* ── Center panel — hashrate (large) ── */
    int16_t CenterMidX = PRO_CENTER_X + PRO_CENTER_W / 2;

    /* Clear center area */
    s_Tft.fillRect(PRO_CENTER_X, PRO_BAR_H + 2,
                   PRO_CENTER_W - 2, SCREEN_H - PRO_BAR_H - 2, PRO_BG);

    /* Big hashrate number */
    FormatHashRate(p_Stats->HashRate, Buf, sizeof(Buf), Unit, sizeof(Unit));
    s_Tft.setTextColor(PRO_ACCENT, PRO_BG);
    s_Tft.setTextDatum(TC_DATUM);

    /* Font 7 is the large 7-segment font (48px), perfect for hashrate */
    s_Tft.drawString(Buf, CenterMidX, PRO_BAR_H + 20, 7);

    /* Unit label to the right of number */
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.setTextSize(1);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.drawString(Unit, CenterMidX, PRO_BAR_H + 75, 2);

    /* Thin separator */
    s_Tft.drawFastHLine(PRO_CENTER_X + 10, PRO_BAR_H + 95,
                        PRO_CENTER_W - 20, PRO_PANEL);

    /* Total hashes below */
    FormatTotalHashes(p_Stats->TotalHashes, Buf, sizeof(Buf));
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.drawString(Buf, CenterMidX - 15, PRO_BAR_H + 105, 2);
    s_Tft.drawString("total", CenterMidX + 30, PRO_BAR_H + 105, 2);

    /* HW/SW breakdown */
    s_Tft.setTextSize(1);
    snprintf(Buf, sizeof(Buf), "HW:%luK SW:%luK",
             (unsigned long)(p_Stats->HashRateHw / 1000),
             (unsigned long)(p_Stats->HashRateSw / 1000));
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.drawString(Buf, CenterMidX, PRO_BAR_H + 130, 1);

    /* Blocks found (only if > 0) */
    if (p_Stats->BlocksFound > 0) {
        snprintf(Buf, sizeof(Buf), "BLOCKS: %lu", (unsigned long)p_Stats->BlocksFound);
        s_Tft.setTextColor(PRO_ALERT, PRO_BG);
        s_Tft.drawString(Buf, CenterMidX, PRO_BAR_H + 150, 2);
    }

    /* ── Right stats panel ── */
    int16_t Ry = PRO_BAR_H + 12;
    int16_t RightMidX = PRO_RIGHT_X + PRO_RIGHT_W / 2;
    int16_t RowH = 48;

    /* Clear right panel interior */
    s_Tft.fillRect(PRO_RIGHT_X, PRO_BAR_H + 2,
                   PRO_RIGHT_W - 1, SCREEN_H - PRO_BAR_H - 3, PRO_BG);
    /* Redraw border */
    s_Tft.drawRect(PRO_RIGHT_X - 1, PRO_BAR_H + 1,
                   PRO_RIGHT_W + 1, SCREEN_H - PRO_BAR_H - 1, PRO_ACCENT);

    /* Shares */
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.drawString("SHARES", RightMidX, Ry, 1);
    snprintf(Buf, sizeof(Buf), "%lu", (unsigned long)p_Stats->SharesAccepted);
    s_Tft.setTextColor(PRO_TEXT, PRO_BG);
    s_Tft.drawString(Buf, RightMidX, Ry + 12, 2);
    Ry += RowH;

    /* Difficulty */
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.drawString("DIFF", RightMidX, Ry, 1);
    FormatDifficulty(p_Stats->Difficulty, Buf, sizeof(Buf));
    s_Tft.setTextColor(PRO_TEXT, PRO_BG);
    s_Tft.drawString(Buf, RightMidX, Ry + 12, 2);
    Ry += RowH;

    /* Templates */
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.drawString("TEMPLATES", RightMidX, Ry, 1);
    snprintf(Buf, sizeof(Buf), "%lu", (unsigned long)p_Stats->Templates);
    s_Tft.setTextColor(PRO_TEXT, PRO_BG);
    s_Tft.drawString(Buf, RightMidX, Ry + 12, 2);
    Ry += RowH;

    /* Best Share */
    s_Tft.setTextColor(PRO_TEXT_DIM, PRO_BG);
    s_Tft.drawString("BEST", RightMidX, Ry, 1);
    FormatDifficulty(p_Stats->BestDiff, Buf, sizeof(Buf));
    s_Tft.setTextColor(PRO_ALERT, PRO_BG);
    s_Tft.drawString(Buf, RightMidX, Ry + 12, 2);

    s_Tft.setTextDatum(TL_DATUM);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  TRINITY LITE THEME
 *
 *  Layout (320x240):
 *  ┌──────────────────────────────────────┐ 0
 *  │ [wifi]     HH:MM          XX°C      │ 24  ← top bar
 *  ├──────────────────────────────────────┤ 25  ← 1px divider
 *  │                                      │
 *  │          742.6 KH/s                  │     ← large center
 *  │           152.6M total               │
 *  │                                      │
 *  │                            Shares XX │
 *  │                            Diff  1.0 │     ← right-aligned stats
 *  │                            Tmpl  42  │
 *  │                                      │
 *  │            [trinity icon]            │     ← quiet signature
 *  └──────────────────────────────────────┘ 240
 * ═══════════════════════════════════════════════════════════════════════ */

#define LITE_BAR_H       24

static void LiteDrawFrame(void)
{
    /* Thin 1px divider below top bar */
    s_Tft.drawFastHLine(0, LITE_BAR_H, SCREEN_W, LITE_ACCENT);

    /* Trinity icon — small, bottom center, quiet signature */
    DrawTrinityIcon(SCREEN_W / 2, 215, 1, LITE_ACCENT);
}

static void LiteDrawStats(const PdqMinerStats_t* p_Stats)
{
    char Buf[32];
    char Unit[8];

    /* ── Top bar ── */
    s_Tft.fillRect(0, 0, SCREEN_W, LITE_BAR_H, LITE_BG);

    /* WiFi icon left */
    DrawWifiIcon(14, 4, LITE_ACCENT, p_Stats->WifiConnected);

    /* Uptime center */
    FormatUptime(p_Stats->Uptime, Buf, sizeof(Buf));
    s_Tft.setTextColor(LITE_TEXT, LITE_BG);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.setTextSize(1);
    s_Tft.drawString(Buf, SCREEN_W / 2, 5, 2);

    /* Temperature right */
    snprintf(Buf, sizeof(Buf), "%.0fC", p_Stats->Temperature);
    uint16_t Tc = TempColor(p_Stats->Temperature, LITE_TEXT_DIM, TFT_ORANGE, TFT_RED);
    s_Tft.setTextColor(Tc, LITE_BG);
    s_Tft.setTextDatum(TR_DATUM);
    s_Tft.drawString(Buf, SCREEN_W - 6, 5, 2);

    /* Redraw divider (in case overdrawn) */
    s_Tft.drawFastHLine(0, LITE_BAR_H, SCREEN_W, LITE_ACCENT);

    /* ── Center hashrate (large, clean) ── */
    /* Clear the main area */
    s_Tft.fillRect(0, LITE_BAR_H + 1, SCREEN_W, SCREEN_H - LITE_BAR_H - 35, LITE_BG);

    FormatHashRate(p_Stats->HashRate, Buf, sizeof(Buf), Unit, sizeof(Unit));

    /* Hashrate — large font 7 centered */
    s_Tft.setTextColor(LITE_TEXT, LITE_BG);
    s_Tft.setTextDatum(TC_DATUM);
    s_Tft.drawString(Buf, SCREEN_W / 2, LITE_BAR_H + 20, 7);

    /* Unit below the number */
    s_Tft.setTextColor(LITE_ACCENT, LITE_BG);
    s_Tft.drawString(Unit, SCREEN_W / 2, LITE_BAR_H + 75, 2);

    /* Total hashes */
    FormatTotalHashes(p_Stats->TotalHashes, Buf, sizeof(Buf));
    s_Tft.setTextColor(LITE_TEXT_DIM, LITE_BG);
    char FullBuf[48];
    snprintf(FullBuf, sizeof(FullBuf), "%s total", Buf);
    s_Tft.drawString(FullBuf, SCREEN_W / 2, LITE_BAR_H + 95, 2);

    /* ── Right-aligned stats (text only, no boxes) ── */
    int16_t Ry = LITE_BAR_H + 120;
    int16_t RowH = 20;
    int16_t LabelX = SCREEN_W - 110;
    int16_t ValueX = SCREEN_W - 10;

    s_Tft.setTextSize(1);

    /* Shares */
    s_Tft.setTextColor(LITE_TEXT_DIM, LITE_BG);
    s_Tft.setTextDatum(TL_DATUM);
    s_Tft.drawString("Shares", LabelX, Ry, 2);
    snprintf(Buf, sizeof(Buf), "%lu", (unsigned long)p_Stats->SharesAccepted);
    s_Tft.setTextColor(LITE_TEXT, LITE_BG);
    s_Tft.setTextDatum(TR_DATUM);
    s_Tft.drawString(Buf, ValueX, Ry, 2);
    Ry += RowH;

    /* Difficulty */
    s_Tft.setTextColor(LITE_TEXT_DIM, LITE_BG);
    s_Tft.setTextDatum(TL_DATUM);
    s_Tft.drawString("Diff", LabelX, Ry, 2);
    FormatDifficulty(p_Stats->Difficulty, Buf, sizeof(Buf));
    s_Tft.setTextColor(LITE_TEXT, LITE_BG);
    s_Tft.setTextDatum(TR_DATUM);
    s_Tft.drawString(Buf, ValueX, Ry, 2);
    Ry += RowH;

    /* Templates */
    s_Tft.setTextColor(LITE_TEXT_DIM, LITE_BG);
    s_Tft.setTextDatum(TL_DATUM);
    s_Tft.drawString("Tmpl", LabelX, Ry, 2);
    snprintf(Buf, sizeof(Buf), "%lu", (unsigned long)p_Stats->Templates);
    s_Tft.setTextColor(LITE_TEXT, LITE_BG);
    s_Tft.setTextDatum(TR_DATUM);
    s_Tft.drawString(Buf, ValueX, Ry, 2);

    /* Blocks badge (if found) */
    if (p_Stats->BlocksFound > 0) {
        Ry += RowH;
        snprintf(Buf, sizeof(Buf), "BLOCK!");
        s_Tft.setTextColor(LITE_ACCENT, LITE_BG);
        s_Tft.setTextDatum(TR_DATUM);
        s_Tft.drawString(Buf, ValueX, Ry, 2);
    }

    /* Redraw Trinity icon (bottom center, quiet signature) */
    s_Tft.fillRect(SCREEN_W / 2 - 20, 205, 40, 30, LITE_BG);
    DrawTrinityIcon(SCREEN_W / 2, 215, 1, LITE_ACCENT);

    s_Tft.setTextDatum(TL_DATUM);
}

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
