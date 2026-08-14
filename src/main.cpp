// ============================================================
//  HeathenHawk Talon5 — main.cpp
//  Touch polling matches the working debug_touch.cpp approach
//  vTaskDelay(1) in loop, continuous polling, no debounce gaps
// ============================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include "pins.h"
#include "src/display/display_driver.h"
#include "src/comms/comms_manager.h"
#include "src/hawk/hawk_pet.h"

#define HH_VERSION  "v1.0.0-talon5"

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
void mode_settings();
void mode_hawk_screen() { HawkPet::drawHawkScreen(); }

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
    { "Camera",       mode_camera,        HH_TEAL    },
    { "GPS Info",     mode_gps_info,      HH_TEAL    },
    { "SD Browser",   mode_sd_browser,    HH_GRAY    },
    { "Settings",     mode_settings,      HH_GRAY    },
};
static const uint8_t MENU_COUNT = sizeof(MENU)/sizeof(MENU[0]);

static uint8_t     menuIndex   = 0;
static bool        sdReady     = false;
static bool        gpsFix      = false;
static bool        c5Ready     = false;
static uint8_t     battPercent = 100;
static Orientation orientation = ORI_PORTRAIT;

// ── Global touch state updated every loop ────────────────────────────────────
static m5::touch_point_t g_tp[5];
static int               g_touchNum   = 0;
static bool              g_prevTouch  = false;
static bool              g_justTapped = false;
static int32_t           g_tapX       = 0;
static int32_t           g_tapY       = 0;

void pollTouch() {
    bool wasDown = (g_touchNum > 0);
    g_touchNum = M5.Lcd.getTouchRaw(g_tp, 5);

    if (g_touchNum > 0) {
        M5.Lcd.convertRawXY(g_tp, g_touchNum);
    }

    // Detect tap = was up, now down
    g_justTapped = (!wasDown && g_touchNum > 0);
    if (g_justTapped) {
        g_tapX = g_tp[0].x;
        g_tapY = g_tp[0].y;
        Serial.printf("[Touch] Tap x=%d y=%d\n", g_tapX, g_tapY);
    }
}

void checkOrientation() {
    auto imu = M5.Imu.getImuData();
    Orientation newOri = abs(imu.accel.x) > abs(imu.accel.y) ?
                         ORI_LANDSCAPE : ORI_PORTRAIT;
    if (newOri != orientation) {
        orientation = newOri;
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
        Display::drawKeyboardHint("↑↓ navigate  |  Enter / tap to launch");
    } else {
        Display::drawTabletMenu(labels, colors, MENU_COUNT, menuIndex);
    }
}

// ── Check if tap hit a menu item ──────────────────────────────────────────────
int8_t tapToMenuItem(int32_t tx, int32_t ty) {
    if (orientation == ORI_LANDSCAPE) {
        if (tx < 320 && ty > 48) {
            int32_t itemH = (Display::height() - 48) / min((int)MENU_COUNT, 10);
            int16_t scroll = max(0, (int)menuIndex - 4);
            int8_t tapped = scroll + (ty - 48) / itemH;
            if (tapped >= 0 && tapped < MENU_COUNT) return tapped;
        } else if (tx >= 320 && ty > 48) {
            return menuIndex;  // detail panel = launch current
        }
    } else {
        int32_t cols   = 2;
        int32_t cardH  = 160;
        int32_t padX   = 20;
        int32_t cardW  = (Display::width() - padX * 3) / cols;
        int32_t startY = 64 + padX;
        if (ty >= startY) {
            int32_t col = (tx - padX) / (cardW + padX);
            int32_t row = (ty - startY) / (cardH + padX);
            if (col >= 0 && col < cols && row >= 0) {
                int8_t idx = row * cols + col;
                if (idx < MENU_COUNT) return idx;
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
    Display::clear();
    renderMenu();
}

void setup() {
    // Disable brownout detector — prevents touch controller
    // from being left uninitialized after brownout reset

    delay(1000);  // Let power rails fully stabilize
    Serial.begin(115200);
    M5.begin();
    M5.Power.setExtOutput(true);  // Enable 5V on Grove/M5Bus
    delay(500);
    
    Serial.println("\n[Talon5] Booting...");

    // Critical touch diagnostic
    Serial.printf("[Touch] driver ptr: %p\n", M5.Display.touch());
    Serial.printf("[Touch] display: %dx%d\n",
                  M5.Display.width(), M5.Display.height());

    // Test touch immediately after M5.begin()
    m5::touch_point_t tp[1];
    int n = M5.Lcd.getTouchRaw(tp, 1);
    Serial.printf("[Touch] Initial poll: %d points\n", n);

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
    else         Display::showToast("2.4GHz mode — C5 not detected", HH_GRAY);

    checkOrientation();
    Display::clear();
    renderMenu();

    Serial.println("[Talon5] Ready — tap the screen!");
}

void loop() {
    M5.update();
    Comms::poll();

    // Poll touch every iteration — same as working debug_touch.cpp
    pollTouch();

    // Handle tap
    if (g_justTapped) {
        int8_t item = tapToMenuItem(g_tapX, g_tapY);
        if (item >= 0) {
            if (item == menuIndex) {
                // Double tap same item = launch
                // Single tap = select
                static int8_t lastTapped = -1;
                static uint32_t lastTapMs = 0;
                if (lastTapped == item && millis() - lastTapMs < 600) {
                    launchMode(item);
                    lastTapped = -1;
                } else {
                    lastTapped = item;
                    lastTapMs = millis();
                    menuIndex = item;
                    renderMenu();
                }
            } else {
                menuIndex = item;
                renderMenu();
            }
        }
        g_justTapped = false;
    }

    // Keyboard
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w' && menuIndex > 0)              { menuIndex--; renderMenu(); }
        if (c == 's' && menuIndex < MENU_COUNT-1)   { menuIndex++; renderMenu(); }
        if (c == '\r' || c == '\n')                 { launchMode(menuIndex); }
    }

    // Periodic checks
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 3000) {
        lastCheck = millis();
        checkOrientation();
        battPercent = (uint8_t)constrain(M5.Power.getBatteryLevel(), 0, 100);
    }

    HawkPet::tick();
    vTaskDelay(1);  // Same as working debug_touch.cpp
}
