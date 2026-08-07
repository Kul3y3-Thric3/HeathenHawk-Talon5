// ============================================================
//  HeathenHawk Talon5 — display/display_driver.cpp
//  M5GFX implementation for Tab5 5" 1280x720 touchscreen
// ============================================================

#include "display_driver.h"
#include <stdarg.h>

static Orientation currentOri = ORI_PORTRAIT;

// ── Status bar height changes per orientation ─────────────────────────────────
#define STATUS_H  48

namespace Display {

void begin() {
    // M5Unified handles display init — just configure defaults
    M5.Display.setRotation(0);
    M5.Display.fillScreen(HH_DARK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(HH_WHITE, HH_DARK);
    M5.Display.setTextDatum(lgfx::TL_DATUM);
    HH_LOG("[Display] Tab5 5\" 1280x720 ready");
}

void setOrientation(Orientation ori) {
    currentOri = ori;
    if (ori == ORI_PORTRAIT) {
        M5.Display.setRotation(0);   // 720 x 1280
    } else {
        M5.Display.setRotation(1);   // 1280 x 720
    }
    M5.Display.fillScreen(HH_DARK);
}

Orientation getOrientation() { return currentOri; }
int32_t width()  { return M5.Display.width();  }
int32_t height() { return M5.Display.height(); }

// ── Primitives ────────────────────────────────────────────────────────────────
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

// ── Text ──────────────────────────────────────────────────────────────────────
void setTextColor(uint32_t fg, uint32_t bg) { M5.Display.setTextColor(fg, bg); }
void setTextSize(float s)  { M5.Display.setTextSize(s); }
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

int32_t textWidth(const char* str)  { return M5.Display.textWidth(str); }
int32_t textHeight()                { return M5.Display.fontHeight(); }

// ── Touch ─────────────────────────────────────────────────────────────────────
TouchEvent getTouch() {
    TouchEvent evt = {0, 0, false, false, false};
    M5.update();
    auto touch = M5.Touch.getDetail();
    evt.x        = touch.x;
    evt.y        = touch.y;
    evt.pressed  = touch.wasPressed();
    evt.released = touch.wasReleased();
    evt.held     = touch.isPressed();
    return evt;
}

bool isTouched() {
    M5.update();
    return M5.Touch.getDetail().isPressed();
}

void waitForTap() {
    while (true) {
        M5.update();
        if (M5.Touch.getDetail().wasPressed()) return;
        delay(20);
    }
}

// ── Status bar ────────────────────────────────────────────────────────────────
void drawStatusBar(const char* mode, bool gps, bool sd, bool c5, uint8_t battery) {
    // Dark gradient bar
    M5.Display.fillRect(0, 0, width(), STATUS_H, HH_DARKCARD);
    M5.Display.drawLine(0, STATUS_H-1, width(), STATUS_H-1, HH_PURPLE);

    // Mode name
    M5.Display.setTextColor(HH_WHITE, HH_DARKCARD);
    M5.Display.setTextSize(1.8f);
    M5.Display.setCursor(16, 12);
    M5.Display.print(mode);

    // Status badges — right side
    int32_t bx = width() - 16;

    // Battery
    uint32_t batCol = battery > 50 ? HH_GREEN : battery > 20 ? HH_AMBER : HH_CORAL;
    char batStr[8]; snprintf(batStr, sizeof(batStr), "%d%%", battery);
    M5.Display.setTextSize(1.2f);
    int32_t batW = M5.Display.textWidth(batStr) + 16;
    bx -= batW;
    M5.Display.fillRoundRect(bx, 10, batW, 28, 6, batCol);
    M5.Display.setTextColor(HH_WHITE, batCol);
    M5.Display.setCursor(bx + 8, 16);
    M5.Display.print(batStr);

    // C5 badge
    bx -= 52;
    uint32_t c5Col = c5 ? HH_TEAL : HH_GRAY;
    M5.Display.fillRoundRect(bx, 10, 44, 28, 6, c5Col);
    M5.Display.setTextColor(HH_WHITE, c5Col);
    M5.Display.setCursor(bx + 6, 16);
    M5.Display.print(c5 ? "5GHz" : "C5?");

    // SD badge
    bx -= 44;
    uint32_t sdCol = sd ? HH_GREEN : HH_GRAY;
    M5.Display.fillRoundRect(bx, 10, 36, 28, 6, sdCol);
    M5.Display.setTextColor(HH_WHITE, sdCol);
    M5.Display.setCursor(bx + 8, 16);
    M5.Display.print("SD");

    // GPS badge
    bx -= 52;
    uint32_t gpsCol = gps ? HH_GREEN : HH_GRAY;
    M5.Display.fillRoundRect(bx, 10, 44, 28, 6, gpsCol);
    M5.Display.setTextColor(HH_WHITE, gpsCol);
    M5.Display.setCursor(bx + 6, 16);
    M5.Display.print(gps ? "GPS" : "GPS?");
}

// ── Card component ────────────────────────────────────────────────────────────
void drawCard(int32_t x, int32_t y, int32_t w, int32_t h,
              const char* title, uint32_t accentColor) {
    M5.Display.fillRoundRect(x, y, w, h, 12, HH_DARKCARD);
    M5.Display.drawRoundRect(x, y, w, h, 12, accentColor);
    M5.Display.fillRect(x, y, w, 4, accentColor);
    if (title && strlen(title) > 0) {
        M5.Display.setTextColor(accentColor, HH_DARKCARD);
        M5.Display.setTextSize(1.3f);
        M5.Display.setCursor(x + 12, y + 10);
        M5.Display.print(title);
    }
}

// ── Scan row ──────────────────────────────────────────────────────────────────
void drawScanRow(int32_t y, int32_t h, const char* label, const char* detail,
                 const char* badge, int8_t rssi, bool highlight, uint32_t color) {
    uint32_t bg = highlight ? color : HH_DARKCARD;
    M5.Display.fillRect(0, y, width(), h, bg);
    M5.Display.drawLine(0, y+h-1, width(), y+h-1, HH_DARK);

    // Label
    M5.Display.setTextColor(highlight ? HH_WHITE : HH_WHITE, bg);
    M5.Display.setTextSize(1.4f);
    M5.Display.setCursor(16, y + 8);
    M5.Display.print(label);

    // Detail
    M5.Display.setTextColor(highlight ? HH_WHITE : HH_LGRAY, bg);
    M5.Display.setTextSize(1.1f);
    M5.Display.setCursor(16, y + 30);
    M5.Display.print(detail);

    // Badge
    if (badge && strlen(badge) > 0) {
        int32_t bw = M5.Display.textWidth(badge) + 16;
        int32_t bx = width() - bw - 80;
        M5.Display.fillRoundRect(bx, y + 10, bw, 24, 6, color);
        M5.Display.setTextColor(HH_WHITE, color);
        M5.Display.setCursor(bx + 8, y + 14);
        M5.Display.print(badge);
    }

    // RSSI bar
    if (rssi != -127) {
        int32_t barW = 60;
        int32_t barH = 10;
        int32_t bx = width() - barW - 12;
        int32_t by = y + h/2 - barH/2;
        int32_t filled = barW * constrain(map(rssi, -100, -20, 0, 100), 0, 100) / 100;
        M5.Display.fillRoundRect(bx, by, barW, barH, 4, HH_GRAY);
        if (filled > 0) M5.Display.fillRoundRect(bx, by, filled, barH, 4, color);
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

// ── Badge ────────────────────────────────────────────────────────────────────
void drawBadge(int32_t x, int32_t y, const char* label, uint32_t color) {
    M5.Display.setTextSize(1.1f);
    int32_t bw = M5.Display.textWidth(label) + 16;
    M5.Display.fillRoundRect(x, y, bw, 28, 8, color);
    M5.Display.setTextColor(HH_WHITE, color);
    M5.Display.setCursor(x + 8, y + 6);
    M5.Display.print(label);
}

// ── Alert popup ───────────────────────────────────────────────────────────────
void showAlert(const char* title, const char* msg, uint32_t color, uint32_t ms) {
    int32_t pw = width() * 7 / 10;
    int32_t ph = 180;
    int32_t px = (width() - pw) / 2;
    int32_t py = (height() - ph) / 2;

    // Shadow
    M5.Display.fillRoundRect(px+4, py+4, pw, ph, 16, 0x000000);
    // Card
    M5.Display.fillRoundRect(px, py, pw, ph, 16, HH_DARKCARD);
    M5.Display.drawRoundRect(px, py, pw, ph, 16, color);
    // Accent top
    M5.Display.fillRect(px, py, pw, 6, color);

    // Title
    M5.Display.setTextColor(color, HH_DARKCARD);
    M5.Display.setTextSize(2.0f);
    M5.Display.setCursor(px + 20, py + 24);
    M5.Display.print(title);

    // Message
    M5.Display.setTextColor(HH_WHITE, HH_DARKCARD);
    M5.Display.setTextSize(1.4f);
    M5.Display.setCursor(px + 20, py + 80);
    M5.Display.print(msg);

    if (ms > 0) delay(ms);
}

// ── Toast notification ────────────────────────────────────────────────────────
void showToast(const char* msg, uint32_t color) {
    int32_t tw = width() * 6 / 10;
    int32_t th = 60;
    int32_t tx = (width() - tw) / 2;
    int32_t ty = height() - th - 20;
    M5.Display.fillRoundRect(tx, ty, tw, th, 12, color);
    M5.Display.setTextColor(HH_WHITE, color);
    M5.Display.setTextSize(1.4f);
    M5.Display.setCursor(tx + 20, ty + 16);
    M5.Display.print(msg);
    delay(1500);
}

// ── Boot animation ────────────────────────────────────────────────────────────
void playBootAnimation() {
    M5.Display.fillScreen(HH_DARK);
    // Sweep scan lines
    for (int32_t y = 0; y < height(); y += 6) {
        M5.Display.fillRect(0, y, width(), 3, HH_PURPLE);
        if (y % 30 == 0) delay(8);
    }
    delay(200);
    M5.Display.fillScreen(HH_DARK);
}

// ── Splash screen ─────────────────────────────────────────────────────────────
void showSplash(const char* version) {
    M5.Display.fillScreen(HH_DARK);

    // Logo block
    int32_t cx = width() / 2;

    M5.Display.setTextColor(HH_PURPLE, HH_DARK);
    M5.Display.setTextSize(5.0f);
    M5.Display.setTextDatum(lgfx::TC_DATUM);
    M5.Display.drawString("HEATHEN", cx, 80);

    M5.Display.setTextColor(HH_AMBER, HH_DARK);
    M5.Display.setTextSize(6.0f);
    M5.Display.drawString("HAWK", cx, 180);

    M5.Display.setTextColor(HH_TEAL, HH_DARK);
    M5.Display.setTextSize(2.0f);
    M5.Display.drawString("TALON5", cx, 300);

    M5.Display.setTextColor(HH_GRAY, HH_DARK);
    M5.Display.setTextSize(1.4f);
    M5.Display.drawString("M5Stack Tab5 Red Team Toolkit", cx, 360);
    M5.Display.drawString(version, cx, 395);

    M5.Display.setTextColor(HH_PURPLE, HH_DARK);
    M5.Display.setTextSize(1.2f);
    M5.Display.drawString("by Kul3y3-Thric3", cx, 440);
    M5.Display.drawString("Heavens Heathens / ProTechTor", cx, 468);

    // Reset datum
    M5.Display.setTextDatum(lgfx::TL_DATUM);
    delay(2000);
}

// ── Tablet menu (portrait mode — grid layout) ─────────────────────────────────
void drawTabletMenu(const char** labels, const uint32_t* colors,
                    uint8_t count, uint8_t selected) {
    M5.Display.fillRect(0, STATUS_H, width(), height()-STATUS_H, HH_DARK);

    int32_t cols  = 2;
    int32_t rows  = (count + cols - 1) / cols;
    int32_t cardW = (width() - 48) / cols;
    int32_t cardH = 120;
    int32_t padX  = 16;
    int32_t padY  = 16;
    int32_t startY = STATUS_H + padY;

    for (uint8_t i = 0; i < count; i++) {
        int32_t col = i % cols;
        int32_t row = i / cols;
        int32_t x = padX + col * (cardW + padX);
        int32_t y = startY + row * (cardH + padY);

        bool sel = (i == selected);
        uint32_t col_color = colors[i];

        // Card background
        M5.Display.fillRoundRect(x, y, cardW, cardH, 12,
                                 sel ? col_color : HH_DARKCARD);
        M5.Display.drawRoundRect(x, y, cardW, cardH, 12, col_color);

        // Top accent
        if (!sel) M5.Display.fillRect(x, y, cardW, 4, col_color);

        // Label
        M5.Display.setTextColor(sel ? HH_WHITE : HH_WHITE,
                                sel ? col_color : HH_DARKCARD);
        M5.Display.setTextSize(1.6f);
        M5.Display.setCursor(x + 16, y + 20);
        M5.Display.print(labels[i]);

        // Selected indicator
        if (sel) {
            M5.Display.fillCircle(x + cardW - 20, y + cardH - 20, 8, HH_WHITE);
        }
    }
}

// ── Cyberdeck menu (landscape mode — side list) ───────────────────────────────
void drawCyberdeckMenu(const char** labels, const uint32_t* colors,
                       uint8_t count, uint8_t selected) {
    M5.Display.fillRect(0, STATUS_H, width(), height()-STATUS_H, HH_DARK);

    int32_t listW = 320;
    int32_t itemH = (height() - STATUS_H) / min((int)count, 10);
    int32_t startY = STATUS_H;

    // Sidebar background
    M5.Display.fillRect(0, STATUS_H, listW, height()-STATUS_H, HH_DARKCARD);
    M5.Display.drawLine(listW, STATUS_H, listW, height(), HH_PURPLE);

    int16_t scroll = max(0, (int)selected - 4);

    for (uint8_t i = scroll; i < count && (i - scroll) < 10; i++) {
        int32_t y = startY + (i - scroll) * itemH;
        bool sel = (i == selected);
        uint32_t c = colors[i];

        M5.Display.fillRect(0, y, listW, itemH, sel ? c : HH_DARKCARD);
        M5.Display.drawLine(0, y+itemH-1, listW, y+itemH-1, HH_DARK);

        // Accent bar
        if (!sel) M5.Display.fillRect(0, y, 4, itemH, c);

        M5.Display.setTextColor(HH_WHITE, sel ? c : HH_DARKCARD);
        M5.Display.setTextSize(1.5f);
        M5.Display.setCursor(sel ? 20 : 16, y + itemH/2 - 10);
        M5.Display.print(labels[i]);
    }

    // Detail panel — right of list
    M5.Display.fillRect(listW+1, STATUS_H, width()-listW-1,
                        height()-STATUS_H, HH_DARK);
    M5.Display.setTextColor(colors[selected], HH_DARK);
    M5.Display.setTextSize(2.5f);
    M5.Display.setCursor(listW + 30, STATUS_H + 40);
    M5.Display.print(labels[selected]);
    M5.Display.setTextColor(HH_GRAY, HH_DARK);
    M5.Display.setTextSize(1.3f);
    M5.Display.setCursor(listW + 30, STATUS_H + 90);
    M5.Display.print("Tap to launch");
    M5.Display.setCursor(listW + 30, STATUS_H + 115);
    M5.Display.print("or press Enter");
}

// ── GPS panel ─────────────────────────────────────────────────────────────────
void drawGPSPanel(double lat, double lon, float speed,
                  uint8_t sats, bool fix) {
    uint32_t fixCol = fix ? HH_GREEN : HH_CORAL;
    char buf[32];

    M5.Display.setTextColor(fixCol, HH_DARK);
    M5.Display.setTextSize(1.5f);
    M5.Display.setCursor(16, STATUS_H + 16);
    M5.Display.print(fix ? "GPS FIXED" : "NO FIX");

    snprintf(buf, sizeof(buf), "Sats: %d", sats);
    M5.Display.setCursor(200, STATUS_H + 16);
    M5.Display.print(buf);

    M5.Display.setTextColor(HH_WHITE, HH_DARK);
    M5.Display.setTextSize(2.0f);
    M5.Display.setCursor(16, STATUS_H + 60);
    snprintf(buf, sizeof(buf), "%.6f", lat);
    M5.Display.print(buf);
    M5.Display.setCursor(16, STATUS_H + 100);
    snprintf(buf, sizeof(buf), "%.6f", lon);
    M5.Display.print(buf);

    M5.Display.setTextColor(HH_GRAY, HH_DARK);
    M5.Display.setTextSize(1.4f);
    M5.Display.setCursor(16, STATUS_H + 145);
    snprintf(buf, sizeof(buf), "%.1f km/h", speed);
    M5.Display.print(buf);
}

// ── Keyboard hint bar ─────────────────────────────────────────────────────────
void drawKeyboardHint(const char* hints) {
    int32_t barH = 36;
    int32_t y = height() - barH;
    M5.Display.fillRect(0, y, width(), barH, HH_DARKCARD);
    M5.Display.drawLine(0, y, width(), y, HH_PURPLE);
    M5.Display.setTextColor(HH_GRAY, HH_DARKCARD);
    M5.Display.setTextSize(1.2f);
    M5.Display.setCursor(16, y + 8);
    M5.Display.print(hints);
}

} // namespace Display
