/**
 * @file display_driver.cpp
 * @brief Display driver using TFT_eSPI for ILI9341/ST7789
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "display_driver.h"

#ifndef PDQ_HEADLESS

#include <TFT_eSPI.h>
#include <Arduino.h>

#define COLOR_BG        TFT_BLACK
#define COLOR_TEXT      TFT_WHITE
#define COLOR_ACCENT    TFT_CYAN
#define COLOR_HASH      TFT_GREEN
#define COLOR_SHARE     TFT_YELLOW
#define COLOR_WARN      TFT_ORANGE
#define COLOR_ERROR     TFT_RED

static TFT_eSPI s_Tft = TFT_eSPI();
static PdqDisplayMode_t s_Mode = PdqDisplayModeStandard;
static uint32_t s_LastUpdate = 0;

static void DrawHeader(void);
static void DrawMiningStats(const PdqMinerStats_t* p_Stats);
static void FormatHashRate(uint32_t HashRate, char* p_Buffer, size_t BufLen);
static void FormatUptime(uint32_t Seconds, char* p_Buffer, size_t BufLen);

extern "C" {

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode)
{
    s_Mode = Mode;
    
    if (Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }
    
    s_Tft.init();
    s_Tft.setRotation(3);
    s_Tft.fillScreen(COLOR_BG);
    
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif
    
    DrawHeader();
    
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
    if (Now - s_LastUpdate < 500) {
        return PdqOk;
    }
    s_LastUpdate = Now;
    
    DrawMiningStats(p_Stats);
    
    return PdqOk;
}

PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2)
{
    if (s_Mode == PdqDisplayModeHeadless) {
        return PdqOk;
    }
    
    s_Tft.fillRect(0, 100, 320, 80, COLOR_BG);
    s_Tft.setTextColor(COLOR_TEXT, COLOR_BG);
    s_Tft.setTextSize(2);
    
    if (p_Line1 != NULL) {
        s_Tft.setCursor(10, 110);
        s_Tft.print(p_Line1);
    }
    
    if (p_Line2 != NULL) {
        s_Tft.setCursor(10, 140);
        s_Tft.print(p_Line2);
    }
    
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

}

static void DrawHeader(void)
{
    s_Tft.fillRect(0, 0, 320, 30, TFT_NAVY);
    s_Tft.setTextColor(COLOR_TEXT, TFT_NAVY);
    s_Tft.setTextSize(2);
    s_Tft.setCursor(10, 7);
    s_Tft.print("PDQminer v");
    s_Tft.print(PDQ_VERSION);
    
    s_Tft.drawLine(0, 30, 320, 30, COLOR_ACCENT);
}

static void DrawMiningStats(const PdqMinerStats_t* p_Stats)
{
    char Buffer[32];
    int16_t Y = 45;
    const int16_t LineHeight = 30;
    
    s_Tft.setTextSize(2);
    
    s_Tft.fillRect(0, Y, 320, LineHeight, COLOR_BG);
    s_Tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    s_Tft.setCursor(10, Y + 5);
    s_Tft.print("Hash: ");
    s_Tft.setTextColor(COLOR_HASH, COLOR_BG);
    FormatHashRate(p_Stats->HashRate, Buffer, sizeof(Buffer));
    s_Tft.print(Buffer);
    Y += LineHeight;
    
    s_Tft.fillRect(0, Y, 320, LineHeight, COLOR_BG);
    s_Tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    s_Tft.setCursor(10, Y + 5);
    s_Tft.print("Shares: ");
    s_Tft.setTextColor(COLOR_SHARE, COLOR_BG);
    snprintf(Buffer, sizeof(Buffer), "%lu", (unsigned long)p_Stats->SharesAccepted);
    s_Tft.print(Buffer);
    if (p_Stats->SharesRejected > 0) {
        s_Tft.setTextColor(COLOR_ERROR, COLOR_BG);
        snprintf(Buffer, sizeof(Buffer), " (%lu rej)", (unsigned long)p_Stats->SharesRejected);
        s_Tft.print(Buffer);
    }
    Y += LineHeight;
    
    s_Tft.fillRect(0, Y, 320, LineHeight, COLOR_BG);
    if (p_Stats->BlocksFound > 0) {
        s_Tft.setTextColor(COLOR_ACCENT, COLOR_BG);
        s_Tft.setCursor(10, Y + 5);
        s_Tft.print("Blocks: ");
        s_Tft.setTextColor(TFT_GOLD, COLOR_BG);
        snprintf(Buffer, sizeof(Buffer), "%lu", (unsigned long)p_Stats->BlocksFound);
        s_Tft.print(Buffer);
    }
    Y += LineHeight;
    
    s_Tft.fillRect(0, Y, 320, LineHeight, COLOR_BG);
    s_Tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    s_Tft.setCursor(10, Y + 5);
    s_Tft.print("Temp: ");
    uint16_t TempColor = COLOR_TEXT;
    if (p_Stats->Temperature > 70.0f) {
        TempColor = COLOR_ERROR;
    } else if (p_Stats->Temperature > 55.0f) {
        TempColor = COLOR_WARN;
    }
    s_Tft.setTextColor(TempColor, COLOR_BG);
    snprintf(Buffer, sizeof(Buffer), "%.1fC", p_Stats->Temperature);
    s_Tft.print(Buffer);
    Y += LineHeight;
    
    s_Tft.fillRect(0, Y, 320, LineHeight, COLOR_BG);
    s_Tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    s_Tft.setCursor(10, Y + 5);
    s_Tft.print("Uptime: ");
    s_Tft.setTextColor(COLOR_TEXT, COLOR_BG);
    FormatUptime(p_Stats->Uptime, Buffer, sizeof(Buffer));
    s_Tft.print(Buffer);
}

static void FormatHashRate(uint32_t HashRate, char* p_Buffer, size_t BufLen)
{
    if (HashRate >= 1000000) {
        snprintf(p_Buffer, BufLen, "%.2f MH/s", HashRate / 1000000.0f);
    } else if (HashRate >= 1000) {
        snprintf(p_Buffer, BufLen, "%.2f KH/s", HashRate / 1000.0f);
    } else {
        snprintf(p_Buffer, BufLen, "%lu H/s", (unsigned long)HashRate);
    }
}

static void FormatUptime(uint32_t Seconds, char* p_Buffer, size_t BufLen)
{
    uint32_t Days = Seconds / 86400;
    uint32_t Hours = (Seconds % 86400) / 3600;
    uint32_t Mins = (Seconds % 3600) / 60;
    
    if (Days > 0) {
        snprintf(p_Buffer, BufLen, "%lud %02luh %02lum", 
                 (unsigned long)Days, (unsigned long)Hours, (unsigned long)Mins);
    } else if (Hours > 0) {
        snprintf(p_Buffer, BufLen, "%luh %02lum", 
                 (unsigned long)Hours, (unsigned long)Mins);
    } else {
        snprintf(p_Buffer, BufLen, "%lum %02lus", 
                 (unsigned long)Mins, (unsigned long)(Seconds % 60));
    }
}

#else

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

}

#endif
