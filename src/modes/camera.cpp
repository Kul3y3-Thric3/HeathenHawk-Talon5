// ============================================================
//  HeathenHawk Talon5 — modes/camera.cpp
//  SC2356 2MP camera — pending M5Unified P4 camera support
// ============================================================
#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>

#define MODE_NAME "Camera"

void mode_camera() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);
    Display::drawCard(40, 100, Display::width()-80, 320, "Camera", HH_TEAL);

    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.6f);
    Display::setCursor(60, 160);
    Display::print("Camera coming soon!");
    Display::setTextSize(1.3f);
    Display::setTextColor(HH_GRAY, HH_DARKCARD);
    Display::setCursor(60, 200);
    Display::print("SC2356 2MP MIPI-CSI camera");
    Display::setCursor(60, 235);
    Display::print("Pending M5Unified P4 support");
    Display::setCursor(60, 285);
    Display::setTextColor(HH_TEAL, HH_DARKCARD);
    Display::print("Tap to go back");

    Display::waitForTap();
}
