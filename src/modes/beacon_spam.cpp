// ============================================================
//  HeathenHawk Talon5 — modes/beacon_spam.cpp
//  WiFi beacon spam via C6 co-processor
// ============================================================
#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>

#define MODE_NAME "Beacon Spam"

void mode_beacon_spam() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);
    Display::drawCard(40, 80, Display::width()-80, 350, "Beacon Spam", HH_AMBER);

    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.5f);
    Display::setCursor(60, 130);
    Display::print("Select spam mode:");

    Display::fillRoundRect(60, 185, Display::width()-120, 70, 16, HH_PURPLE);
    Display::setTextColor(HH_WHITE, HH_PURPLE);
    Display::setTextSize(1.6f);
    Display::setCursor(80, 210);
    Display::print("📢  Random Funny SSIDs");

    Display::fillRoundRect(60, 270, Display::width()-120, 70, 16, HH_TEAL);
    Display::setTextColor(HH_WHITE, HH_TEAL);
    Display::setCursor(80, 295);
    Display::print("📋  Sequential List");

    Display::fillRoundRect(60, 355, Display::width()-120, 60, 16, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setCursor(80, 375);
    Display::print("✕  Cancel");

    const char* mode = nullptr;
    while (!mode) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.y >= 185 && evt.y <= 255) mode = "random";
            else if (evt.y >= 270 && evt.y <= 340) mode = "sequential";
            else if (evt.y >= 355) return;
        }
        delay(30);
    }

    Comms::startBeaconSpam(mode);
    uint32_t framesSent = 0;
    uint32_t lastRender = 0;

    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);

    while (true) {
        M5.update(); framesSent++;
        if (framesSent % 100 == 0) HawkPet::feed(FEED_BEACON_SPAM, 1);

        if (millis() - lastRender > 600) {
            lastRender = millis();
            Display::fillRect(0, 48, Display::width(), Display::height()-48, HH_DARK);
            Display::setTextColor(HH_AMBER, HH_DARK);
            Display::setTextSize(5.0f);
            Display::setCursor(40, 120);
            char buf[16]; snprintf(buf, sizeof(buf), "%lu", framesSent);
            Display::print(buf);
            Display::setTextColor(HH_GRAY, HH_DARK);
            Display::setTextSize(1.5f);
            Display::setCursor(40, 240);
            Display::print("beacons sent");
            Display::setCursor(40, 280);
            Display::printf("Mode: %s", mode);
        }

        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed() && evt.x < 30) break;
        delay(10);
    }

    Comms::stopAll();
    char msg[32]; snprintf(msg, sizeof(msg), "%lu beacons sent", framesSent);
    Display::showAlert("Stopped", msg, HH_GRAY, 2000);
}
