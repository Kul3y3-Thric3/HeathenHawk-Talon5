// ============================================================
//  HeathenHawk Talon5 — modes/gps_info.cpp
//  Live GPS info display
// ============================================================
#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <TinyGPSPlus.h>

#define MODE_NAME "GPS Info"

void mode_gps_info() {
    TinyGPSPlus gps;
    Serial2.begin(115200, SERIAL_8N1, M5BUS_UART_RX, M5BUS_UART_TX);

    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.5f);
    Display::setCursor(40, Display::height()/2 - 20);
    Display::print("Waiting for GPS fix...");
    Display::setCursor(40, Display::height()/2 + 20);
    Display::print("Connect GPS to M5Bus UART");

    uint32_t lastRender = 0;
    bool hasFix = false;

    while (true) {
        M5.update();

        while (Serial2.available()) gps.encode(Serial2.read());

        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed() && evt.x < 30) break;

        if (millis() - lastRender > 1000) {
            lastRender = millis();
            hasFix = gps.location.isValid() && gps.satellites.value() >= 3;

            Display::clear(HH_DARK);
            Display::drawStatusBar(MODE_NAME, hasFix, false, false, 100);

            Display::drawGPSPanel(
                gps.location.isValid() ? gps.location.lat() : 0,
                gps.location.isValid() ? gps.location.lng() : 0,
                gps.speed.isValid() ? gps.speed.kmph() : 0,
                gps.satellites.isValid() ? gps.satellites.value() : 0,
                hasFix
            );

            // Extra stats
            Display::setTextColor(HH_GRAY, HH_DARK);
            Display::setTextSize(1.3f);
            Display::setCursor(16, 280);
            Display::printf("HDOP: %.1f", gps.hdop.isValid() ? gps.hdop.hdop() : 99.0f);
            Display::setCursor(16, 310);
            Display::printf("Alt: %.0fm", gps.altitude.isValid() ? gps.altitude.meters() : 0.0f);
            Display::setCursor(16, 340);
            Display::printf("Sentences: %lu", gps.passedChecksum());
            Display::setCursor(16, 370);
            Display::printf("Failed: %lu", gps.failedChecksum());
            Display::setCursor(16, 410);
            Display::setTextColor(HH_GRAY, HH_DARK);
            Display::print("Swipe left to exit");

            if (hasFix) HawkPet::feed(FEED_GPS, 1);
        }

        HawkPet::tick(); delay(50);
    }
}
