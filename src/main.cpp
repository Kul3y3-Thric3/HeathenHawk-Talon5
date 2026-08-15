// ============================================================
//  HeathenHawk Talon5 — HeathenHawk_Talon5.ino
//  M5Stack Tab5 Red Team Toolkit
//  by Kul3y3-Thric3 / Heavens Heathens / ProTechTor
// ============================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include "pins.h"
#include "src/display/display_driver.h"
#include "src/comms/comms_manager.h"
#include "src/hawk/hawk_pet.h"

#define HH_VERSION  "v1.0.0-talon5"

// ── Forward declarations ──────────────────────────────────────────────────────
void mode_wifi_scanner();
void mode_ble_scanner();
void mode_foxhunter();
void mode_flockyou();
void mode_skyspy();
void mode_wardriving();
void mode_evil_portal();
void mode_deauth();
void mode_beacon_spam();
void mode_ble_spam();
void mode_camera();
void mode_gps_info();
void mode_sd_browser();
void mode_rfid();
void mode_nfc();
void mode_settings();
void mode_hawk_screen() { HawkPet::drawHawkScreen(); }

// ── Menu ──────────────────────────────────────────────────────────────────────
struct MenuItem {
    const char* label;
    void (*handler)();
    uint32_t    color;
};

static const MenuItem MENU[] = {
    { "HawkBird",     mode_hawk_screen,   HH_PURPLE  },
    { "WiFi Scanner", mode_wifi_scanner,  HH_TEAL    },
    { "BLE Scanner",  mode_ble_scanner,   HH_PURPLE  },
    { "Foxhunter",    mode_foxhunter,     HH_AMBER   },
    { "Flock-You",    mode_flockyou,      HH_CORAL   },
    { "Sky Spy",      mode_skyspy,        HH_BLUE    },
    { "Wardriving",   mode_wardriving,    HH_GREEN   },
    { "Evil Portal",  mode_evil_portal,   HH_PINK    },
    { "Deauth",       mode_deauth,        HH_RED     },
    { "Beacon Spam",  mode_beacon_spam,   HH_AMBER   },
    { "BLE Spam",     mode_ble_spam,      HH_PURPLE  },
    { "RFID Scanner", mode_rfid,          HH_GREEN   },
    { "NFC",          mode_nfc,           HH_TEAL    },
    { "Camera",       mode_camera,        HH_TEAL    },
    { "GPS Info",     mode_gps_info,      HH_TEAL    },
    { "SD Browser",   mode_sd_browser,    HH_GRAY    },
    { "Settings",     mode_settings,      HH_GRAY    },
};
static const uint8_t MENU_COUNT = sizeof(MENU)/sizeof(MENU[0]);

// ── State ─────────────────────────────────────────────────────────────────────
static uint8_t     menuIndex   = 0;
static int16_t     menuScroll  = 0;  // rows scrolled (portrait grid)
static bool        sdReady     = false;
static bool        gpsFix      = false;
static bool        c5Ready     = false;
static uint8_t     battPercent = 100;
static Orientation orientation = ORI_PORTRAIT;

// ── Touch state ───────────────────────────────────────────────────────────────
static m5::touch_point_t g_tp[5];
static int               g_touchNum   = 0;
static bool              g_justTapped = false;
static int32_t           g_tapX       = 0;
static int32_t           g_tapY       = 0;

void pollTouch() {
    bool wasDown = (g_touchNum > 0);
    g_touchNum = M5.Lcd.getTouchRaw(g_tp, 5);
    g_justTapped = (!wasDown && g_touchNum > 0);
    if (g_justTapped) {
        g_tapX = g_tp[0].x;
        g_tapY = g_tp[0].y;
    }
}

void checkOrientation() {
    auto imu = M5.Imu.getImuData();
    Orientation newOri = abs(imu.accel.x) > abs(imu.accel.y) ?
                         ORI_LANDSCAPE : ORI_PORTRAIT;
    if (newOri != orientation) {
        orientation = newOri;
        menuScroll = 0;
        Display::setOrientation(orientation);
    }
}

void renderMenu() {
    static const char* labels[MENU_COUNT];
    static uint32_t    colors[MENU_COUNT];
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        labels[i] = MENU[i].label;
        colors[i] = MENU[i].color;
    }
    Display::drawStatusBar("MENU", gpsFix, sdReady, c5Ready, battPercent);

    if (orientation == ORI_LANDSCAPE) {
        Display::drawCyberdeckMenu(labels, colors, MENU_COUNT, menuIndex);
        Display::drawKeyboardHint("↑↓ navigate  |  Enter to launch");
    } else {
        // Portrait grid with scroll
        int32_t cols   = 2;
        int32_t cardH  = 160;
        int32_t padX   = 20;
        int32_t startY = 64 + padX;
        int32_t visRows = (Display::height() - startY) / (cardH + padX);
        int32_t maxScroll = (int32_t)((MENU_COUNT + cols - 1) / cols) - visRows;
        if (maxScroll < 0) maxScroll = 0;
        if (menuScroll > maxScroll) menuScroll = maxScroll;

        // Scroll indicators
        Display::fillRect(0, 64, Display::width(),
                          Display::height()-64, HH_DARK);

        if (menuScroll > 0) {
            Display::setTextColor(HH_GRAY, HH_DARK);
            Display::setTextSize(2.5f);
            Display::setCursor(Display::width()/2 - 30, startY - 14);
            Display::print("▲ scroll up");
        }
        if (menuScroll < maxScroll) {
            Display::setTextColor(HH_GRAY, HH_DARK);
            Display::setTextSize(2.5f);
            Display::setCursor(Display::width()/2 - 40,
                               Display::height() - 36);
            Display::print("▼ scroll down");
        }

        // Draw visible cards
        int32_t startItem = menuScroll * cols;
        int32_t y = startY;
        for (int32_t row = 0; row < visRows && startItem + row*cols < MENU_COUNT; row++) {
            for (int32_t col = 0; col < cols; col++) {
                int32_t idx = startItem + row*cols + col;
                if (idx >= MENU_COUNT) break;
                int32_t cardW = (Display::width() - padX * 3) / cols;
                int32_t x = padX + col * (cardW + padX);

                bool sel = (idx == menuIndex);
                uint32_t c = MENU[idx].color;

                Display::fillRoundRect(x, y, cardW, cardH, 16,
                                       sel ? c : HH_DARKCARD);
                Display::drawRoundRect(x, y, cardW, cardH, 16, c);
                if (!sel) Display::fillRect(x, y, cardW, 6, c);

                Display::setTextColor(HH_WHITE, sel ? c : HH_DARKCARD);
                Display::setTextSize(2.4f);
                Display::setCursor(x + 20, y + 24);
                Display::print(MENU[idx].label);

                if (sel) {
                    Display::setTextSize(2.2f);
                    Display::setCursor(x + 20, y + 90);
                    Display::print("Tap again to launch");
                }
            }
            y += cardH + padX;
        }
    }
}

int8_t tapToMenuItem(int32_t tx, int32_t ty) {
    if (orientation == ORI_LANDSCAPE) {
        if (tx < 320 && ty > 48) {
            int32_t itemH = (Display::height() - 48) / min((int)MENU_COUNT, 10);
            int16_t scroll = max(0, (int)menuIndex - 4);
            int8_t tapped = scroll + (ty - 48) / itemH;
            if (tapped >= 0 && tapped < MENU_COUNT) return tapped;
        } else if (tx >= 320 && ty > 48) {
            return menuIndex;
        }
    } else {
        int32_t cols   = 2;
        int32_t cardW  = (Display::width() - 20 * 3) / cols;
        int32_t cardH  = 160;
        int32_t padX   = 20;
        int32_t startY = 64 + padX;
        if (ty >= startY) {
            int32_t col = (tx - padX) / (cardW + padX);
            int32_t row = (ty - startY) / (cardH + padX);
            if (col >= 0 && col < cols && row >= 0) {
                int8_t idx = menuScroll * cols + row * cols + col;
                if (idx >= 0 && idx < MENU_COUNT) return idx;
            }
        }
    }
    return -1;
}

void launchMode(uint8_t idx) {
    Display::clear();
    Display::drawStatusBar(MENU[idx].label, gpsFix, sdReady, c5Ready, battPercent);
    M5.Speaker.tone(880, 60); delay(80); M5.Speaker.stop();
    if (MENU[idx].handler) MENU[idx].handler();
    M5.Speaker.tone(440, 60); delay(70);
    M5.Speaker.tone(330, 80); delay(100);
    M5.Speaker.stop();
    menuScroll = 0;
    Display::clear();
    renderMenu();
}

void setup() {
    delay(500);
    M5.begin();
    M5.Power.setExtOutput(true);
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[Talon5] Booting...");
    Serial.printf("[Touch] driver ptr: %p\n", M5.Display.touch());

    Display::begin();
    Display::playBootAnimation();
    Display::showSplash(HH_VERSION);

    M5.Imu.init();

    Display::clear();
    int32_t bw = Display::width()*3/4;
    int32_t bx = (Display::width()-bw)/2;
    int32_t by = Display::height()/2 - 10;

    sdReady = SD.begin();
    Display::drawProgressBar(bx, by, bw, 20, 40, HH_TEAL);

    Comms::begin();
    c5Ready = Comms::c5Available();
    Display::drawProgressBar(bx, by, bw, 20, 70, HH_TEAL);

    HawkPet::begin();
    Display::drawProgressBar(bx, by, bw, 20, 90, HH_TEAL);

    M5.Speaker.setVolume(128);
    uint16_t notes[] = {523, 659, 784, 1047};
    for (auto n : notes) { M5.Speaker.tone(n, 100); delay(120); }
    M5.Speaker.stop();

    Display::drawProgressBar(bx, by, bw, 20, 100, HH_TEAL);
    delay(300);

    if (c5Ready) Display::showToast("C5 detected — Dual-band active!", HH_TEAL);
    else         Display::showToast("Ready!", HH_PURPLE);

    checkOrientation();
    Display::clear();
    renderMenu();
    Serial.println("[Talon5] Ready!");
}

void loop() {
    M5.update();
    Comms::poll();
    pollTouch();

    // Swipe scroll for portrait grid
    if (orientation == ORI_PORTRAIT) {
        static int32_t swipeStartY = 0;
        static bool    swiping     = false;

        if (g_touchNum > 0 && !swiping) {
            swipeStartY = g_tp[0].y;
            swiping = true;
        }
        if (g_touchNum == 0 && swiping) {
            int32_t dy = swipeStartY - g_tp[0].y;
            if (abs(dy) > 80) {
                int32_t cols   = 2;
                int32_t cardH  = 160;
                int32_t padX   = 20;
                int32_t startY = 64 + padX;
                int32_t visRows = (Display::height() - startY) / (cardH + padX);
                int32_t maxScroll = (int32_t)((MENU_COUNT + cols - 1) / cols) - visRows;
        if (maxScroll < 0) maxScroll = 0;
                menuScroll = (int16_t)constrain(menuScroll + (dy > 0 ? 1 : -1),
                                                0, maxScroll);
                renderMenu();
            }
            swiping = false;
        }
    }

    if (g_justTapped) {
        int8_t item = tapToMenuItem(g_tapX, g_tapY);
        if (item >= 0) {
            if (item == menuIndex) {
                static int8_t  lastTapped = -1;
                static uint32_t lastTapMs = 0;
                if (lastTapped == item && millis() - lastTapMs < 600) {
                    launchMode(item);
                    lastTapped = -1;
                } else {
                    lastTapped = item;
                    lastTapMs  = millis();
                    menuIndex  = item;
                    renderMenu();
                }
            } else {
                menuIndex = item;
                renderMenu();
            }
        }
        g_justTapped = false;
    }

    // Keyboard navigation
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w' && menuIndex > 0)            { menuIndex--; renderMenu(); }
        if (c == 's' && menuIndex < MENU_COUNT-1) { menuIndex++; renderMenu(); }
        if (c == '\r' || c == '\n')               { launchMode(menuIndex); }
    }

    // Periodic checks
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 3000) {
        lastCheck = millis();
        checkOrientation();
        battPercent = (uint8_t)constrain(M5.Power.getBatteryLevel(), 0, 100);
    }

    HawkPet::tick();
    vTaskDelay(1);
}
