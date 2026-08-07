// ============================================================
//  HeathenHawk Talon5 — main.cpp
//  M5Stack Tab5 Red Team Toolkit
//  by Kul3y3-Thric3 / Heavens Heathens / ProTechTor
//
//  Architecture:
//    ESP32-P4  — this file — UI, orchestration, touch/keyboard
//    ESP32-C6  — built-in WiFi 6 + BLE co-processor
//    ESP32-C5  — optional 5GHz co-processor via M5Bus UART
// ============================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <SD.h>
#include "pins.h"
#include "display/display_driver.h"
#include "comms/comms_manager.h"
#include "hawk/hawk_pet.h"

#define HH_VERSION  "v1.0.0-talon5"

// ── Mode handlers (forward declarations) ─────────────────────────────────────
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
void mode_gps_info();
void mode_camera();
void mode_sd_browser();
void mode_settings();
void mode_hawk_screen() { HawkPet::drawHawkScreen(); }

// ── Menu definition ───────────────────────────────────────────────────────────
struct MenuItem {
    const char* label;
    const char* subtitle;
    void (*handler)();
    uint32_t    color;
    const char* icon;
};

static const MenuItem MENU[] = {
    { "HawkBird",     "Your companion",      mode_hawk_screen,   HH_PURPLE, "🦅" },
    { "WiFi Scanner", "Dual-band scan",       mode_wifi_scanner,  HH_TEAL,   "📡" },
    { "BLE Scanner",  "Passive BLE sniff",   mode_ble_scanner,   HH_PURPLE, "📶" },
    { "Foxhunter",    "RSSI proximity",       mode_foxhunter,     HH_AMBER,  "🦊" },
    { "Flock-You",    "ALPR detection",       mode_flockyou,      HH_CORAL,  "📷" },
    { "Sky Spy",      "Drone Remote ID",      mode_skyspy,        HH_BLUE,   "🛸" },
    { "Wardriving",   "GPS + WiFi + Wigle",   mode_wardriving,    HH_GREEN,  "🗺️" },
    { "Evil Portal",  "Captive portal",       mode_evil_portal,   HH_PINK,   "🕸️" },
    { "Deauth",       "Auth testing",         mode_deauth,        HH_RED,    "⚡" },
    { "Beacon Spam",  "SSID flooding",        mode_beacon_spam,   HH_AMBER,  "📢" },
    { "BLE Spam",     "Pairing popups",       mode_ble_spam,      HH_PURPLE, "💬" },
    { "Camera",       "Visual recon",         mode_camera,        HH_TEAL,   "📸" },
    { "GPS Info",     "Location data",        mode_gps_info,      HH_TEAL,   "📍" },
    { "SD Browser",   "File manager",         mode_sd_browser,    HH_GRAY,   "💾" },
    { "Settings",     "Configure device",     mode_settings,      HH_GRAY,   "⚙️"  },
};
static const uint8_t MENU_COUNT = sizeof(MENU)/sizeof(MENU[0]);

// ── Global state ──────────────────────────────────────────────────────────────
static uint8_t     menuIndex    = 0;
static bool        inMenu       = true;
static bool        sdReady      = false;
static bool        gpsFix       = false;
static bool        c5Ready      = false;
static uint8_t     battPercent  = 100;
static Orientation orientation  = ORI_PORTRAIT;
static bool        kbdMode      = false;

// ── IMU orientation detection ─────────────────────────────────────────────────
void checkOrientation() {
    auto imu = M5.Imu.getImuData();
    float ax = imu.accel.x;
    float ay = imu.accel.y;

    Orientation newOri;
    if (abs(ax) > abs(ay)) {
        newOri = ORI_LANDSCAPE;
    } else {
        newOri = ORI_PORTRAIT;
    }

    if (newOri != orientation) {
        orientation = newOri;
        Display::setOrientation(orientation);
        HH_LOGF("[IMU] Orientation: %s\n",
                orientation == ORI_LANDSCAPE ? "Landscape/Cyberdeck" : "Portrait/Tablet");
    }
}

// ── Keyboard handler ──────────────────────────────────────────────────────────
bool handleKeyboard() {
    // Tab5 keyboard attachment sends HID over USB-C side port
    // Arrow keys, Enter, Escape mapped to navigation
    if (Serial.available()) {
        char c = Serial.read();
        switch (c) {
            case 'w': case 'W': case 0x41:  // Up arrow
                if (menuIndex > 0) menuIndex--;
                return true;
            case 's': case 'S': case 0x42:  // Down arrow
                if (menuIndex < MENU_COUNT-1) menuIndex++;
                return true;
            case '\r': case '\n':            // Enter
                inMenu = false;
                return true;
            case 0x1B:                       // Escape
                inMenu = true;
                return true;
        }
    }
    return false;
}

// ── Menu rendering ────────────────────────────────────────────────────────────
void renderMenu() {
    static const char* labels[MENU_COUNT];
    static uint32_t    colors[MENU_COUNT];
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        labels[i] = MENU[i].label;
        colors[i] = MENU[i].color;
    }

    Display::drawStatusBar("MENU", gpsFix, sdReady, c5Ready, battPercent);

    if (orientation == ORI_LANDSCAPE || kbdMode) {
        Display::drawCyberdeckMenu(labels, colors, MENU_COUNT, menuIndex);
        Display::drawKeyboardHint(
            "↑↓ navigate  |  Enter / tap to launch  |  Esc back  |  Tab rotate"
        );
    } else {
        Display::drawTabletMenu(labels, colors, MENU_COUNT, menuIndex);
    }
}

// ── Touch menu navigation ─────────────────────────────────────────────────────
bool handleTouch() {
    auto evt = Display::getTouch();
    if (!evt.pressed) return false;

    if (orientation == ORI_LANDSCAPE) {
        // Sidebar list — left 320px
        if (evt.x < 320) {
            int32_t relY = evt.y - 48;
            int32_t itemH = (Display::height() - 48) / min((int)MENU_COUNT, 10);
            if (relY >= 0) {
                uint8_t tapped = menuIndex - max(0, (int)menuIndex - 4) +
                                 relY / itemH;
                if (tapped < MENU_COUNT) {
                    if (tapped == menuIndex) {
                        inMenu = false;
                    } else {
                        menuIndex = tapped;
                    }
                    return true;
                }
            }
        } else {
            // Detail panel tap = launch
            inMenu = false;
            return true;
        }
    } else {
        // Portrait grid
        int32_t cols  = 2;
        int32_t cardW = (Display::width() - 48) / cols;
        int32_t cardH = 120;
        int32_t padX  = 16;
        int32_t startY = 48 + 16;

        int32_t col = (evt.x - padX) / (cardW + padX);
        int32_t row = (evt.y - startY) / (cardH + padX);

        if (col >= 0 && col < cols && row >= 0) {
            uint8_t idx = row * cols + col;
            if (idx < MENU_COUNT) {
                if (idx == menuIndex) {
                    inMenu = false;
                } else {
                    menuIndex = idx;
                }
                return true;
            }
        }
    }
    return false;
}

// ── Battery monitoring ────────────────────────────────────────────────────────
void updateBattery() {
    auto power = M5.Power.getBatteryLevel();
    battPercent = (uint8_t)constrain(power, 0, 100);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    HH_LOG("\n[Talon5] Booting HeathenHawk Talon5...");

    Display::begin();
    Display::playBootAnimation();
    Display::showSplash(HH_VERSION);

    // Progress bar boot sequence
    Display::clear();
    Display::drawProgressBar(
        Display::width()/8, Display::height()/2 - 20,
        Display::width()*3/4, 20, 0, HH_TEAL);

    // Init IMU for orientation
    M5.Imu.init();
    Display::drawProgressBar(
        Display::width()/8, Display::height()/2 - 20,
        Display::width()*3/4, 20, 20, HH_TEAL);

    // Init SD
    sdReady = SD.begin();
    HH_LOG(sdReady ? "[SD] Ready" : "[SD] No card");
    Display::drawProgressBar(
        Display::width()/8, Display::height()/2 - 20,
        Display::width()*3/4, 20, 40, HH_TEAL);

    // Init comms (C6 + optional C5)
    Comms::begin();
    c5Ready = Comms::c5Available();
    HH_LOGF("[Comms] C6: OK  C5: %s\n", c5Ready ? "OK (5GHz!)" : "not detected");
    Display::drawProgressBar(
        Display::width()/8, Display::height()/2 - 20,
        Display::width()*3/4, 20, 70, HH_TEAL);

    // Init HawkBird
    HawkPet::begin();
    Display::drawProgressBar(
        Display::width()/8, Display::height()/2 - 20,
        Display::width()*3/4, 20, 90, HH_TEAL);

    // Boot sound
    M5.Speaker.setVolume(128);
    M5.Speaker.tone(523, 80); delay(90);
    M5.Speaker.tone(659, 80); delay(90);
    M5.Speaker.tone(784, 80); delay(90);
    M5.Speaker.tone(1047, 150); delay(200);
    M5.Speaker.stop();

    Display::drawProgressBar(
        Display::width()/8, Display::height()/2 - 20,
        Display::width()*3/4, 20, 100, HH_TEAL);
    delay(400);

    // C5 status toast
    if (c5Ready) {
        Display::showToast("C5 detected — Dual-band active!", HH_TEAL);
    } else {
        Display::showToast("2.4GHz mode — C5 not detected", HH_GRAY);
    }

    // Check initial orientation
    checkOrientation();

    inMenu = true;
    Display::clear();
    renderMenu();

    HH_LOG("[Talon5] Boot complete");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    M5.update();
    Comms::poll();

    // Check orientation every 2 seconds
    static uint32_t lastOriCheck = 0;
    if (millis() - lastOriCheck > 2000) {
        lastOriCheck = millis();
        checkOrientation();
        updateBattery();
    }

    bool needsRedraw = false;

    if (inMenu) {
        // Touch navigation
        if (handleTouch()) needsRedraw = true;

        // Keyboard navigation
        if (handleKeyboard()) needsRedraw = true;

        if (!inMenu) {
            // Launch selected mode
            Display::clear();
            Display::drawStatusBar(MENU[menuIndex].label,
                                   gpsFix, sdReady, c5Ready, battPercent);
            M5.Speaker.tone(880, 60);
            delay(80);
            M5.Speaker.stop();

            if (MENU[menuIndex].handler) {
                MENU[menuIndex].handler();
            }

            // Back to menu
            M5.Speaker.tone(440, 60); delay(70);
            M5.Speaker.tone(330, 80); delay(100);
            M5.Speaker.stop();

            inMenu = true;
            needsRedraw = true;
        }

        if (needsRedraw) {
            Display::clear();
            renderMenu();
        }
    }

    HawkPet::tick();
    delay(16);  // ~60fps
}
