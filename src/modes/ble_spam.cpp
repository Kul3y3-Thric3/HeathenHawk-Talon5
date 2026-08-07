// ============================================================
//  HeathenHawk Talon5 — modes/ble_spam.cpp
//  BLE advertisement spam via C6 co-processor
// ============================================================
#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>

#define MODE_NAME "BLE Spam"

void mode_ble_spam() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);
    Display::drawCard(40, 80, Display::width()-80, 400, "BLE Spam", HH_PURPLE);

    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.5f);
    Display::setCursor(60, 130);
    Display::print("Target platform:");

    struct { const char* label; const char* target; uint32_t color; } opts[] = {
        {"🍎  Apple devices",      "apple",   HH_BLUE},
        {"🤖  Android / Samsung",  "android", HH_GREEN},
        {"🪟  Windows Swift Pair", "windows", HH_PURPLE},
        {"📱  All platforms",      "all",     HH_TEAL},
    };

    for (int i = 0; i < 4; i++) {
        Display::fillRoundRect(60, 185 + i*85, Display::width()-120, 70, 16, opts[i].color);
        Display::setTextColor(HH_WHITE, opts[i].color);
        Display::setTextSize(1.6f);
        Display::setCursor(80, 210 + i*85);
        Display::print(opts[i].label);
    }

    const char* target = nullptr;
    while (!target) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            for (int i = 0; i < 4; i++) {
                if (evt.y >= 185+i*85 && evt.y <= 255+i*85) { target = opts[i].target; break; }
            }
            if (evt.x < 30) return;
        }
        delay(30);
    }

    Comms::startBLESpam(target);
    uint32_t adsSent = 0;
    uint32_t lastRender = 0;

    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);

    while (true) {
        M5.update(); adsSent++;
        if (adsSent % 50 == 0) HawkPet::feed(FEED_BLE_SPAM, 1);

        if (millis() - lastRender > 600) {
            lastRender = millis();
            Display::fillRect(0, 48, Display::width(), Display::height()-48, HH_DARK);
            Display::setTextColor(HH_PURPLE, HH_DARK);
            Display::setTextSize(5.0f);
            Display::setCursor(40, 120);
            char buf[16]; snprintf(buf, sizeof(buf), "%lu", adsSent);
            Display::print(buf);
            Display::setTextColor(HH_GRAY, HH_DARK);
            Display::setTextSize(1.5f);
            Display::setCursor(40, 240);
            Display::print("BLE ads sent");
            Display::setCursor(40, 280);
            Display::printf("Target: %s", target);
        }

        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed() && evt.x < 30) break;
        delay(10);
    }

    Comms::stopAll();
    char msg[32]; snprintf(msg, sizeof(msg), "%lu ads sent", adsSent);
    Display::showAlert("Stopped", msg, HH_GRAY, 2000);
}
