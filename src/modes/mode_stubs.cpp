// ============================================================
//  HeathenHawk Talon5 — modes/mode_stubs.cpp
//  Placeholder implementations for modes being built
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>

// ── Generic stub UI ───────────────────────────────────────────────────────────
static void stubMode(const char* name, uint32_t color) {
    Display::clear(HH_DARK);
    Display::drawStatusBar(name, false, false, false, 100);

    Display::drawCard(40, 80, Display::width()-80, 300, name, color);

    Display::setTextColor(color, HH_DARKCARD);
    Display::setTextSize(2.0f);
    Display::setCursor(60, 130);
    Display::print("Coming Soon");

    Display::setTextColor(HH_GRAY, HH_DARKCARD);
    Display::setTextSize(1.4f);
    Display::setCursor(60, 185);
    Display::print("This mode is being built.");
    Display::setCursor(60, 215);
    Display::print("Tap anywhere to go back.");

    Display::waitForTap();
}

// ── Stubs — replace with real implementations as built ───────────────────────
void mode_ble_scanner()  { stubMode("BLE Scanner",  HH_PURPLE); }
void mode_foxhunter()    { stubMode("Foxhunter",    HH_AMBER);  }
void mode_flockyou()     { stubMode("Flock-You",    HH_CORAL);  }
void mode_skyspy()       { stubMode("Sky Spy",      HH_BLUE);   }
void mode_wardriving()   { stubMode("Wardriving",   HH_GREEN);  }
void mode_evil_portal()  { stubMode("Evil Portal",  HH_PINK);   }
void mode_deauth()       { stubMode("Deauth",       HH_RED);    }
void mode_beacon_spam()  { stubMode("Beacon Spam",  HH_AMBER);  }
void mode_ble_spam()     { stubMode("BLE Spam",     HH_PURPLE); }
void mode_camera()       { stubMode("Camera",       HH_TEAL);   }
void mode_gps_info()     { stubMode("GPS Info",     HH_TEAL);   }
void mode_sd_browser()   { stubMode("SD Browser",   HH_GRAY);   }
void mode_settings()     { stubMode("Settings",     HH_GRAY);   }
