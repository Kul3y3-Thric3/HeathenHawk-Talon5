// ============================================================
//  HeathenHawk Talon5 — display/display_driver.cpp
//  Fixed: proper text scaling for 1280x720, sprite buffering
//  to eliminate flicker, larger touch targets
// ============================================================

#include "display_driver.h"
#include <stdarg.h>

static Orientation currentOri = ORI_PORTRAIT;
static lgfx::LGFX_Sprite sprite(&M5.Display);
static bool spriteInit = false;

#define STATUS_H 64

// Scale factor for 1280x720 display
#define TS(x) ((x) * 2.2f)  // text scale
#define PS(x) ((x) * 2)     // pixel scale

namespace Display {

void begin() {
    M5.Display.setRotation(0);
    M5.Display.fillScreen(HH_DARK);
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(HH_WHITE, HH_DARK);
    M5.Display.setTextDatum(lgfx::TL_DATUM);
    HH_LOG("[Display] Tab5 1280x720 ready");
}

void setOrientation(Orientation ori) {
    currentOri = ori;
    M5.Display.setRotation(ori == ORI_LANDSCAPE ? 1 : 0);
    M5.Display.fillScreen(HH_DARK);
}

Orientation getOrientation() { return currentOri; }
int32_t width()  { return M5.Display.width();  }
int32_t height() { return M5.Display.height(); }

void clear(uint32_t color) { M5.Display.fillScreen(color); }

void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    M5.Display.fillRect(x, y, w, h, color);
}
void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    M5.Display.drawRect(x, y, w, h, color);
}
void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    M5.Display.fillRoundRect(x, y, w, h, r, color);
}
void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    M5.Display.drawRoundRect(x, y, w, h, r, color);
}
void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    M5.Display.drawLine(x0, y0, x1, y1, color);
}
void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color) {
    M5.Display.fillCircle(x, y, r, color);
}
void drawCircle(int32_t x, int32_t y, int32_t r, uint32_t color) {
    M5.Display.drawCircle(x, y, r, color);
}

void setTextColor(uint32_t fg, uint32_t bg) { M5.Display.setTextColor(fg, bg); }
void setTextSize(float s)  { M5.Display.setTextSize(s * 2.0f); }  // scale up
void setCursor(int32_t x, int32_t y) { M5.Display.setCursor(x, y); }
void setFont(const lgfx::IFont* font) { M5.Display.setFont(font); }
void print(const char* str) { M5.Display.print(str); }

void printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    M5.Display.print(buf);
}

int32_t textWidth(const char* str) { return M5.Display.textWidth(str); }
int32_t textHeight()               { return M5.Display.fontHeight(); }

// ── Touch ─────────────────────────────────────────────────────────────────────
TouchEvent getTouch() {
    static bool wasDown = false;
    TouchEvent evt = {0, 0, false, false, false};
    m5::touch_point_t tp[1];
    int num = M5.Lcd.getTouchRaw(tp, 1);
    bool isDown = (num > 0);
    if (isDown) {
        evt.x = tp[0].x;
        evt.y = tp[0].y;
    }
    evt.pressed  = isDown && !wasDown;
    evt.released = !isDown && wasDown;
    evt.held     = isDown;
    wasDown = isDown;
    return evt;
}

bool isTouched() {
    m5::touch_point_t tp[1];
    return M5.Lcd.getTouchRaw(tp, 1) > 0;
}

void waitForTap() {
    m5::touch_point_t tp[1];
    while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
    while (M5.Lcd.getTouchRaw(tp, 1) == 0) delay(10);
    while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
}

// ── Status bar ────────────────────────────────────────────────────────────────
void drawStatusBar(const char* mode, bool gps, bool sd, bool c5, uint8_t battery) {
    M5.Display.fillRect(0, 0, width(), STATUS_H, HH_DARKCARD);
    M5.Display.drawLine(0, STATUS_H-1, width(), STATUS_H-1, HH_PURPLE);

    M5.Display.setTextColor(HH_WHITE, HH_DARKCARD);
    M5.Display.setTextSize(3.5f);
    M5.Display.setCursor(20, 14);
    M5.Display.print(mode);

    int32_t bx = width() - 20;

    // Battery
    uint32_t batCol = battery > 50 ? HH_GREEN : battery > 20 ? HH_AMBER : HH_CORAL;
    char batStr[8]; snprintf(batStr, sizeof(batStr), "%d%%", battery);
    M5.Display.setTextSize(2.5f);
    int32_t batW = M5.Display.textWidth(batStr) + 24;
    bx -= batW;
    M5.Display.fillRoundRect(bx, 10, batW, 44, 8, batCol);
    M5.Display.setTextColor(HH_WHITE, batCol);
    M5.Display.setCursor(bx + 12, 18);
    M5.Display.print(batStr);

    // C5
    bx -= 100;
    uint32_t c5Col = c5 ? HH_TEAL : HH_GRAY;
    M5.Display.fillRoundRect(bx, 10, 90, 44, 8, c5Col);
    M5.Display.setTextColor(HH_WHITE, c5Col);
    M5.Display.setCursor(bx + 10, 18);
    M5.Display.print(c5 ? "5GHz" : "C5?");

    // SD
    bx -= 80;
    uint32_t sdCol = sd ? HH_GREEN : HH_GRAY;
    M5.Display.fillRoundRect(bx, 10, 70, 44, 8, sdCol);
    M5.Display.setTextColor(HH_WHITE, sdCol);
    M5.Display.setCursor(bx + 14, 18);
    M5.Display.print("SD");

    // GPS
    bx -= 100;
    uint32_t gpsCol = gps ? HH_GREEN : HH_GRAY;
    M5.Display.fillRoundRect(bx, 10, 90, 44, 8, gpsCol);
    M5.Display.setTextColor(HH_WHITE, gpsCol);
    M5.Display.setCursor(bx + 10, 18);
    M5.Display.print(gps ? "GPS" : "GPS?");
}

// ── Card ──────────────────────────────────────────────────────────────────────
void drawCard(int32_t x, int32_t y, int32_t w, int32_t h,
              const char* title, uint32_t accentColor) {
    M5.Display.fillRoundRect(x, y, w, h, 16, HH_DARKCARD);
    M5.Display.drawRoundRect(x, y, w, h, 16, accentColor);
    M5.Display.fillRect(x, y, w, 6, accentColor);
    if (title && strlen(title) > 0) {
        M5.Display.setTextColor(accentColor, HH_DARKCARD);
        M5.Display.setTextSize(3.0f);
        M5.Display.setCursor(x + 16, y + 14);
        M5.Display.print(title);
    }
}

// ── Scan row ──────────────────────────────────────────────────────────────────
void drawScanRow(int32_t y, int32_t h, const char* label, const char* detail,
                 const char* badge, int8_t rssi, bool highlight, uint32_t color) {
    uint32_t bg = highlight ? color : HH_DARKCARD;
    M5.Display.fillRect(0, y, width(), h, bg);
    M5.Display.drawLine(0, y+h-1, width(), y+h-1, HH_DARK);

    M5.Display.setTextColor(HH_WHITE, bg);
    M5.Display.setTextSize(3.0f);
    M5.Display.setCursor(20, y + 12);
    M5.Display.print(label);

    M5.Display.setTextColor(highlight ? HH_WHITE : HH_LGRAY, bg);
    M5.Display.setTextSize(2.2f);
    M5.Display.setCursor(20, y + h/2 + 4);
    M5.Display.print(detail);

    if (badge && strlen(badge) > 0) {
        M5.Display.setTextSize(2.0f);
        int32_t bw = M5.Display.textWidth(badge) + 24;
        int32_t bx = width() - bw - 120;
        M5.Display.fillRoundRect(bx, y + 14, bw, 36, 8, color);
        M5.Display.setTextColor(HH_WHITE, color);
        M5.Display.setCursor(bx + 12, y + 20);
        M5.Display.print(badge);
    }

    if (rssi != -127) {
        int32_t barW = 90;
        int32_t barH = 16;
        int32_t bx = width() - barW - 16;
        int32_t by = y + h/2 - barH/2;
        int32_t filled = barW * constrain(map(rssi, -100, -20, 0, 100), 0, 100) / 100;
        M5.Display.fillRoundRect(bx, by, barW, barH, 6, HH_GRAY);
        if (filled > 0) M5.Display.fillRoundRect(bx, by, filled, barH, 6, color);
    }
}

// ── RSSI bar ──────────────────────────────────────────────────────────────────
void drawRSSIBar(int32_t x, int32_t y, int32_t w, int32_t h,
                 int8_t rssi, uint32_t color) {
    int32_t filled = (rssi == -127) ? 0 :
        w * constrain(map(rssi, -100, -20, 0, 100), 0, 100) / 100;
    M5.Display.fillRoundRect(x, y, w, h, h/2, HH_GRAY);
    if (filled > 0) M5.Display.fillRoundRect(x, y, filled, h, h/2, color);
}

// ── Progress bar ──────────────────────────────────────────────────────────────
void drawProgressBar(int32_t x, int32_t y, int32_t w, int32_t h,
                     uint8_t pct, uint32_t color) {
    M5.Display.fillRoundRect(x, y, w, h, h/2, HH_GRAY);
    int32_t filled = w * pct / 100;
    if (filled > 0) M5.Display.fillRoundRect(x, y, filled, h, h/2, color);
}

// ── Badge ─────────────────────────────────────────────────────────────────────
void drawBadge(int32_t x, int32_t y, const char* label, uint32_t color) {
    M5.Display.setTextSize(2.5f);
    int32_t bw = M5.Display.textWidth(label) + 24;
    M5.Display.fillRoundRect(x, y, bw, 44, 10, color);
    M5.Display.setTextColor(HH_WHITE, color);
    M5.Display.setCursor(x + 12, y + 10);
    M5.Display.print(label);
}

// ── Alert ─────────────────────────────────────────────────────────────────────
void showAlert(const char* title, const char* msg, uint32_t color, uint32_t ms) {
    int32_t pw = width() * 3 / 4;
    int32_t ph = 280;
    int32_t px = (width() - pw) / 2;
    int32_t py = (height() - ph) / 2;

    M5.Display.fillRoundRect(px+6, py+6, pw, ph, 20, 0x000000);
    M5.Display.fillRoundRect(px, py, pw, ph, 20, HH_DARKCARD);
    M5.Display.drawRoundRect(px, py, pw, ph, 20, color);
    M5.Display.fillRect(px, py, pw, 8, color);

    M5.Display.setTextColor(color, HH_DARKCARD);
    M5.Display.setTextSize(4.0f);
    M5.Display.setCursor(px + 30, py + 30);
    M5.Display.print(title);

    M5.Display.setTextColor(HH_WHITE, HH_DARKCARD);
    M5.Display.setTextSize(3.0f);
    M5.Display.setCursor(px + 30, py + 130);
    M5.Display.print(msg);

    if (ms > 0) delay(ms);
}

// ── Toast ─────────────────────────────────────────────────────────────────────
void showToast(const char* msg, uint32_t color) {
    int32_t tw = width() * 2 / 3;
    int32_t th = 80;
    int32_t tx = (width() - tw) / 2;
    int32_t ty = height() - th - 30;
    M5.Display.fillRoundRect(tx, ty, tw, th, 16, color);
    M5.Display.setTextColor(HH_WHITE, color);
    M5.Display.setTextSize(3.0f);
    M5.Display.setCursor(tx + 24, ty + 20);
    M5.Display.print(msg);
    delay(1500);
}

// ── Boot animation ────────────────────────────────────────────────────────────
void playBootAnimation() {
    M5.Display.fillScreen(HH_DARK);
    for (int32_t y = 0; y < height(); y += 8) {
        M5.Display.fillRect(0, y, width(), 4, HH_PURPLE);
        if (y % 40 == 0) delay(8);
    }
    delay(200);
    M5.Display.fillScreen(HH_DARK);
}

// ── Splash ────────────────────────────────────────────────────────────────────
void showSplash(const char* version) {
    M5.Display.fillScreen(HH_DARK);
    int32_t cx = width() / 2;

    M5.Display.setTextDatum(lgfx::TC_DATUM);
    M5.Display.setTextColor(HH_PURPLE, HH_DARK);
    M5.Display.setTextSize(9.0f);
    M5.Display.drawString("HEATHEN", cx, 100);

    M5.Display.setTextColor(HH_AMBER, HH_DARK);
    M5.Display.setTextSize(11.0f);
    M5.Display.drawString("HAWK", cx, 260);

    M5.Display.setTextColor(HH_TEAL, HH_DARK);
    M5.Display.setTextSize(4.0f);
    M5.Display.drawString("TALON5", cx, 480);

    M5.Display.setTextColor(HH_GRAY, HH_DARK);
    M5.Display.setTextSize(2.8f);
    M5.Display.drawString("M5Stack Tab5 Red Team Toolkit", cx, 580);
    M5.Display.drawString(version, cx, 640);

    M5.Display.setTextColor(HH_PURPLE, HH_DARK);
    M5.Display.setTextSize(2.4f);
    M5.Display.drawString("by Kul3y3-Thric3", cx, 720);
    M5.Display.drawString("Heavens Heathens / ProTechTor", cx, 770);

    M5.Display.setTextDatum(lgfx::TL_DATUM);
    delay(2000);
}

// ── Tablet menu — portrait 2-col grid with LARGE cards ───────────────────────
void drawTabletMenu(const char** labels, const uint32_t* colors,
                    uint8_t count, uint8_t selected) {
    M5.Display.fillRect(0, STATUS_H, width(), height()-STATUS_H, HH_DARK);

    int32_t cols   = 2;
    int32_t padX   = 20;
    int32_t padY   = 16;
    int32_t cardW  = (width() - padX * 3) / cols;
    int32_t cardH  = 160;  // tall enough to tap easily
    int32_t startY = STATUS_H + padY;

    for (uint8_t i = 0; i < count; i++) {
        int32_t col = i % cols;
        int32_t row = i / cols;
        int32_t x = padX + col * (cardW + padX);
        int32_t y = startY + row * (cardH + padY);

        bool sel = (i == selected);
        uint32_t c = colors[i];

        M5.Display.fillRoundRect(x, y, cardW, cardH, 16,
                                 sel ? c : HH_DARKCARD);
        M5.Display.drawRoundRect(x, y, cardW, cardH, 16, c);
        if (!sel) M5.Display.fillRect(x, y, cardW, 6, c);

        M5.Display.setTextColor(HH_WHITE, sel ? c : HH_DARKCARD);
        M5.Display.setTextSize(3.2f);
        M5.Display.setCursor(x + 20, y + 24);
        M5.Display.print(labels[i]);

        if (sel) {
            M5.Display.setTextSize(2.2f);
            M5.Display.setTextColor(HH_WHITE, c);
            M5.Display.setCursor(x + 20, y + 90);
            M5.Display.print("Tap again to launch");
        }
    }
}

// ── Cyberdeck menu — landscape sidebar ───────────────────────────────────────
void drawCyberdeckMenu(const char** labels, const uint32_t* colors,
                       uint8_t count, uint8_t selected) {
    M5.Display.fillRect(0, STATUS_H, width(), height()-STATUS_H, HH_DARK);

    int32_t listW = 440;
    int32_t itemH = (height() - STATUS_H - 50) / min((int)count, 10);
    int32_t startY = STATUS_H;

    M5.Display.fillRect(0, STATUS_H, listW, height()-STATUS_H, HH_DARKCARD);
    M5.Display.drawLine(listW, STATUS_H, listW, height(), HH_PURPLE);

    int16_t scroll = max(0, (int)selected - 4);

    for (uint8_t i = scroll; i < count && (i - scroll) < 10; i++) {
        int32_t y = startY + (i - scroll) * itemH;
        bool sel = (i == selected);
        uint32_t c = colors[i];

        M5.Display.fillRect(0, y, listW, itemH, sel ? c : HH_DARKCARD);
        M5.Display.drawLine(0, y+itemH-1, listW, y+itemH-1, HH_DARK);
        if (!sel) M5.Display.fillRect(0, y, 8, itemH, c);

        M5.Display.setTextColor(HH_WHITE, sel ? c : HH_DARKCARD);
        M5.Display.setTextSize(3.0f);
        M5.Display.setCursor(sel ? 28 : 24, y + itemH/2 - 14);
        M5.Display.print(labels[i]);
    }

    // Detail panel
    M5.Display.fillRect(listW+1, STATUS_H, width()-listW-1,
                        height()-STATUS_H, HH_DARK);
    M5.Display.setTextColor(colors[selected], HH_DARK);
    M5.Display.setTextSize(5.0f);
    M5.Display.setCursor(listW + 40, STATUS_H + 60);
    M5.Display.print(labels[selected]);
    M5.Display.setTextColor(HH_GRAY, HH_DARK);
    M5.Display.setTextSize(2.8f);
    M5.Display.setCursor(listW + 40, STATUS_H + 160);
    M5.Display.print("Tap to launch");
}

// ── GPS panel ─────────────────────────────────────────────────────────────────
void drawGPSPanel(double lat, double lon, float speed, uint8_t sats, bool fix) {
    uint32_t fixCol = fix ? HH_GREEN : HH_CORAL;
    char buf[32];

    M5.Display.setTextColor(fixCol, HH_DARK);
    M5.Display.setTextSize(3.0f);
    M5.Display.setCursor(20, STATUS_H + 20);
    M5.Display.print(fix ? "GPS FIXED" : "NO FIX");

    snprintf(buf, sizeof(buf), "Sats: %d", sats);
    M5.Display.setCursor(300, STATUS_H + 20);
    M5.Display.print(buf);

    M5.Display.setTextColor(HH_WHITE, HH_DARK);
    M5.Display.setTextSize(3.5f);
    M5.Display.setCursor(20, STATUS_H + 90);
    snprintf(buf, sizeof(buf), "%.6f", lat);
    M5.Display.print(buf);
    M5.Display.setCursor(20, STATUS_H + 160);
    snprintf(buf, sizeof(buf), "%.6f", lon);
    M5.Display.print(buf);

    M5.Display.setTextColor(HH_GRAY, HH_DARK);
    M5.Display.setTextSize(2.8f);
    M5.Display.setCursor(20, STATUS_H + 230);
    snprintf(buf, sizeof(buf), "%.1f km/h", speed);
    M5.Display.print(buf);
}

// ── Keyboard hint bar ─────────────────────────────────────────────────────────
void drawKeyboardHint(const char* hints) {
    int32_t barH = 50;
    int32_t y = height() - barH;
    M5.Display.fillRect(0, y, width(), barH, HH_DARKCARD);
    M5.Display.drawLine(0, y, width(), y, HH_PURPLE);
    M5.Display.setTextColor(HH_GRAY, HH_DARKCARD);
    M5.Display.setTextSize(2.2f);
    M5.Display.setCursor(20, y + 12);
    M5.Display.print(hints);
}

} // namespace Display
