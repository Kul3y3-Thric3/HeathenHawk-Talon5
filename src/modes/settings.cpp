// ============================================================
//  HeathenHawk Talon5 — modes/settings.cpp
//  Settings menu with touch UI
//  Load config from SD card /config.txt
// ============================================================
#include "../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <SD.h>

#define MODE_NAME "Settings"

struct SettingItem { const char* label; const char* subtitle; uint32_t color; };
static const SettingItem ITEMS[] = {
    { "Wigle Credentials", "API key + username",    HH_TEAL   },
    { "Upload WiFi",       "SSID + password",       HH_BLUE   },
    { "HawkBird Name",     "Rename your companion", HH_AMBER  },
    { "Reset HawkBird",    "Wipe all XP and stages",HH_CORAL  },
    { "Orientation",       "Auto / Portrait / Land",HH_PURPLE },
    { "Device Info",       "Hardware + firmware",   HH_GRAY   },
    { "Clear All Logs",    "Delete SD log files",   HH_CORAL  },
    { "Load config.txt",   "Read settings from SD", HH_GREEN  },
};
static const uint8_t ITEM_COUNT = sizeof(ITEMS)/sizeof(ITEMS[0]);

void loadSDConfig() {
    if (!SD.exists("/config.txt")) {
        Display::showToast("No config.txt on SD", HH_CORAL);
        return;
    }
    File f = SD.open("/config.txt", FILE_READ);
    if (!f) return;

    Preferences prefs;
    prefs.begin("wigle", false);
    int loaded = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("#") || line.length() == 0) continue;
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq+1);
        key.trim(); val.trim();
        if      (key == "WIGLE_USER") { prefs.putString("user", val.c_str()); loaded++; }
        else if (key == "WIGLE_KEY")  { prefs.putString("key",  val.c_str()); loaded++; }
        else if (key == "HAWK_NAME")  { HawkPet::setName(val.c_str()); loaded++; }
    }
    prefs.end();
    f.close();
    SD.remove("/config.txt");

    char msg[32]; snprintf(msg, sizeof(msg), "%d settings loaded", loaded);
    Display::showAlert("Config Loaded", msg, HH_GREEN, 2000);
}

void showDeviceInfo() {
    Display::clear(HH_DARK);
    Display::drawStatusBar("Device Info", false, false, false, 100);
    Display::drawCard(16, 60, Display::width()-32, 500, "HeathenHawk Talon5", HH_GRAY);

    auto row = [](int32_t y, const char* lbl, const char* val) {
        Display::setTextColor(HH_GRAY, HH_DARKCARD);
        Display::setTextSize(1.3f);
        Display::setCursor(36, y);
        Display::print(lbl);
        Display::setTextColor(HH_WHITE, HH_DARKCARD);
        Display::setCursor(240, y);
        Display::print(val);
    };

    char buf[32];
    row(110, "Board",    "M5Stack Tab5");
    row(145, "MCU",      "ESP32-P4 360MHz");
    row(180, "RAM",      "32MB PSRAM");
    row(215, "Flash",    "16MB");
    row(250, "Display",  "5\" 1280x720 IPS");
    row(285, "Wireless", "C6 (built-in) + C5 (opt)");
    row(320, "Firmware", "Talon5 v1.0.0");
    snprintf(buf, sizeof(buf), "%s", HawkPet::getStageName());
    row(355, "Hawk Stage", buf);
    snprintf(buf, sizeof(buf), "%lu XP", HawkPet::getXP());
    row(390, "Hawk XP",  buf);
    snprintf(buf, sizeof(buf), "%lu KB", ESP.getFreeHeap()/1024);
    row(425, "Free Heap", buf);

    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setCursor(40, Display::height()-40);
    Display::print("Tap to go back");
    Display::waitForTap();
}

void mode_settings() {
    bool sdMounted = SD.begin();

    auto render = [&]() {
        Display::clear(HH_DARK);
        Display::drawStatusBar(MODE_NAME, false, sdMounted, false, 100);
        Display::fillRect(0, 48, Display::width(), 36, HH_DARKCARD);
        Display::setTextColor(HH_GRAY, HH_DARKCARD);
        Display::setTextSize(1.3f);
        Display::setCursor(16, 60);
        Display::print("HeathenHawk Talon5 Configuration");

        int32_t y = 90;
        int32_t rh = 72;
        for (uint8_t i = 0; i < ITEM_COUNT; i++) {
            Display::fillRoundRect(16, y, Display::width()-32, rh-4, 8, HH_DARKCARD);
            Display::fillRect(16, y, 6, rh-4, ITEMS[i].color);
            Display::setTextColor(HH_WHITE, HH_DARKCARD);
            Display::setTextSize(1.5f);
            Display::setCursor(36, y+12);
            Display::print(ITEMS[i].label);
            Display::setTextColor(HH_GRAY, HH_DARKCARD);
            Display::setTextSize(1.2f);
            Display::setCursor(36, y+38);
            Display::print(ITEMS[i].subtitle);
            y += rh;
        }
    };

    render();

    while (true) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;
            if (evt.y >= 90) {
                uint8_t idx = (evt.y - 90) / 72;
                if (idx >= ITEM_COUNT) continue;

                switch (idx) {
                    case 3: // Reset hawk
                        Display::showAlert("Reset HawkBird?", "All XP will be lost", HH_CORAL, 0);
                        Display::waitForTap();
                        HawkPet::reset();
                        Display::showToast("HawkBird reset!", HH_CORAL);
                        break;
                    case 5: // Device info
                        showDeviceInfo();
                        break;
                    case 6: // Clear logs
                        if (sdMounted) {
                            const char* logs[] = {"/wifi_scan.csv","/ble_scan.csv",
                                                  "/wigle_log.csv","/skyspy_log.csv",
                                                  "/flockyou_log.csv","/portal_creds.txt"};
                            int del = 0;
                            for (auto& l : logs) if (SD.exists(l)) { SD.remove(l); del++; }
                            char msg[32]; snprintf(msg, sizeof(msg), "%d files deleted", del);
                            Display::showToast(msg, HH_GREEN);
                        }
                        break;
                    case 7: // Load config
                        if (sdMounted) loadSDConfig();
                        else Display::showToast("No SD card", HH_CORAL);
                        break;
                    default:
                        Display::showToast("Edit via config.txt on SD", HH_GRAY);
                        break;
                }
                render();
            }
        }
        HawkPet::tick(); delay(16);
    }
}
