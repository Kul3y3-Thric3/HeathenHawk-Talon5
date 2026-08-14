// ============================================================
//  HeathenHawk Talon5 — display/display_driver.h
//  M5GFX display abstraction for Tab5 5" 1280x720 touchscreen
//  Touch via M5.Lcd.getTouchRaw() — works on all Tab5 revisions
//  including GT911, ST7123, and ST7121
// ============================================================

#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "../../pins.h"

// ── Color palette (RGB888) ────────────────────────────────────────────────────
#define HH_DARK      0x0A0A12
#define HH_WHITE     0xFFFFFF
#define HH_GRAY      0x666677
#define HH_LGRAY     0xAAAAAB
#define HH_GREEN     0x00E87A
#define HH_TEAL      0x00C8C8
#define HH_BLUE      0x2878FF
#define HH_PURPLE    0x8844FF
#define HH_AMBER     0xFFAA00
#define HH_CORAL     0xFF4444
#define HH_PINK      0xFF44AA
#define HH_RED       0xFF2222
#define HH_BLACK     0x000000
#define HH_DARKCARD  0x12121E
#define HH_ACCENT    0x8844FF

// ── Touch event ───────────────────────────────────────────────────────────────
struct TouchEvent {
    int32_t x;
    int32_t y;
    bool    pressed;
    bool    released;
    bool    held;
};

// ── Orientation ───────────────────────────────────────────────────────────────
enum Orientation {
    ORI_PORTRAIT  = 0,
    ORI_LANDSCAPE = 1,
};

namespace Display {
    void         begin();
    void         setOrientation(Orientation ori);
    Orientation  getOrientation();
    int32_t      width();
    int32_t      height();

    void         clear(uint32_t color = HH_DARK);
    void         fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void         drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void         fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h,
                               int32_t r, uint32_t color);
    void         drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h,
                               int32_t r, uint32_t color);
    void         drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void         fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color);
    void         drawCircle(int32_t x, int32_t y, int32_t r, uint32_t color);

    void         setTextColor(uint32_t fg, uint32_t bg = HH_DARK);
    void         setTextSize(float s);
    void         setCursor(int32_t x, int32_t y);
    void         setFont(const lgfx::IFont* font);
    void         print(const char* str);
    void         printf(const char* fmt, ...);
    int32_t      textWidth(const char* str);
    int32_t      textHeight();

    // Touch — uses M5.Lcd.getTouchRaw() for all Tab5 hardware revisions
    TouchEvent   getTouch();
    bool         isTouched();
    void         waitForTap();

    void         drawStatusBar(const char* mode, bool gps, bool sd,
                               bool c5, uint8_t battery);
    void         drawCard(int32_t x, int32_t y, int32_t w, int32_t h,
                          const char* title, uint32_t accentColor);
    void         drawScanRow(int32_t y, int32_t h, const char* label,
                             const char* detail, const char* badge,
                             int8_t rssi, bool highlight, uint32_t color);
    void         drawRSSIBar(int32_t x, int32_t y, int32_t w, int32_t h,
                             int8_t rssi, uint32_t color);
    void         drawProgressBar(int32_t x, int32_t y, int32_t w, int32_t h,
                                 uint8_t pct, uint32_t color);
    void         drawBadge(int32_t x, int32_t y, const char* label, uint32_t color);
    void         showAlert(const char* title, const char* msg,
                           uint32_t color, uint32_t ms);
    void         showToast(const char* msg, uint32_t color = HH_PURPLE);
    void         playBootAnimation();
    void         showSplash(const char* version);
    void         drawTabletMenu(const char** labels, const uint32_t* colors,
                                uint8_t count, uint8_t selected);
    void         drawCyberdeckMenu(const char** labels, const uint32_t* colors,
                                   uint8_t count, uint8_t selected);
    void         drawGPSPanel(double lat, double lon, float speed,
                              uint8_t sats, bool fix);
    void         drawKeyboardHint(const char* hints);
}
