// ============================================================
//  HeathenHawk Talon5 — modes/foxhunter.cpp
//  RSSI proximity tracker — large visual signal meter on 5"
//  BLE and WiFi target hunting with speaker proximity audio
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>

#define MODE_NAME "Foxhunter"
#define NO_SIGNAL -127

static int8_t  targetRSSI = NO_SIGNAL;
static int8_t  peakRSSI   = NO_SIGNAL;
static char    targetName[32] = {0};
static char    targetMAC[18]  = {0};
static bool    huntingBLE     = true;
static uint32_t lastBeepMs   = 0;

// ── Signal quality ────────────────────────────────────────────────────────────
const char* sigLabel(int8_t r) {
    if (r == NO_SIGNAL) return "NO SIGNAL";
    if (r > -40)        return "VERY CLOSE!";
    if (r > -55)        return "CLOSE";
    if (r > -65)        return "NEARBY";
    if (r > -75)        return "MODERATE";
    if (r > -85)        return "WEAK";
    return                     "FAR";
}
uint32_t sigColor(int8_t r) {
    if (r == NO_SIGNAL) return HH_GRAY;
    if (r > -50)        return HH_GREEN;
    if (r > -65)        return HH_TEAL;
    if (r > -75)        return HH_AMBER;
    return                     HH_CORAL;
}

// ── Big circular signal meter ─────────────────────────────────────────────────
void drawSignalMeter(int8_t rssi) {
    int32_t cx = Display::width() / 2;
    int32_t cy = Display::height() / 2 + 20;
    int32_t maxR = min(Display::width(), Display::height()) / 3;

    // Background rings
    for (int32_t r = maxR; r > 0; r -= maxR/5) {
        Display::drawCircle(cx, cy, r, HH_GRAY);
    }

    if (rssi == NO_SIGNAL) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(2.0f);
        Display::setCursor(cx - 80, cy - 20);
        Display::print("NO SIGNAL");
        return;
    }

    // Fill rings based on signal strength
    uint8_t pct = constrain(map(rssi, -100, -20, 0, 100), 0, 100);
    uint32_t col = sigColor(rssi);
    int32_t fillR = maxR * pct / 100;

    for (int32_t r = fillR; r > 0; r -= 4) {
        Display::fillCircle(cx, cy, r, col);
    }

    // Center circle — white
    Display::fillCircle(cx, cy, 20, HH_WHITE);

    // dBm text
    Display::setTextColor(HH_WHITE, HH_DARK);
    Display::setTextSize(3.0f);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", rssi);
    int32_t tw = Display::textWidth(buf);
    Display::setCursor(cx - tw/2, cy + maxR + 20);
    Display::print(buf);
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.4f);
    Display::setCursor(cx - 20, cy + maxR + 60);
    Display::print("dBm");
}

// ── Render screen ─────────────────────────────────────────────────────────────
void renderFoxhunter() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);

    // Target info
    Display::setTextColor(HH_AMBER, HH_DARK);
    Display::setTextSize(1.6f);
    Display::setCursor(16, 60);
    Display::print(strlen(targetName) > 0 ? targetName : "Hunting...");

    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.2f);
    Display::setCursor(16, 88);
    Display::printf("%s  |  %s", huntingBLE ? "BLE" : "WiFi",
                    strlen(targetMAC) > 0 ? targetMAC : "Any device");

    // Signal label
    uint32_t col = sigColor(targetRSSI);
    Display::setTextColor(col, HH_DARK);
    Display::setTextSize(2.5f);
    int32_t lw = Display::textWidth(sigLabel(targetRSSI));
    Display::setCursor(Display::width()/2 - lw/2, 120);
    Display::print(sigLabel(targetRSSI));

    // Big circular meter
    drawSignalMeter(targetRSSI);

    // Peak
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.3f);
    Display::setCursor(16, Display::height() - 80);
    if (peakRSSI != NO_SIGNAL) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Peak: %d dBm", peakRSSI);
        Display::print(buf);
    }

    // Back hint
    Display::setCursor(16, Display::height() - 50);
    Display::print("Swipe left to exit");
}

// ── BLE callback ─────────────────────────────────────────────────────────────
void onFoxBLE(const BLEResult& r) {
    bool match = (strlen(targetMAC) == 0) ||
                 (strcasecmp(r.mac, targetMAC) == 0);
    if (!match) return;

    targetRSSI = r.rssi;
    if (r.rssi > peakRSSI || peakRSSI == NO_SIGNAL) peakRSSI = r.rssi;
    if (strlen(targetName) == 0 && strlen(r.name) > 0) {
        strlcpy(targetName, r.name, sizeof(targetName));
    }
    HawkPet::feed(FEED_FOXHUNTER, 1);
}

// ── WiFi callback ─────────────────────────────────────────────────────────────
void onFoxWiFi(const WiFiResult& r) {
    bool match = (strlen(targetMAC) == 0) ||
                 (strcasecmp(r.bssid, targetMAC) == 0);
    if (!match) return;

    targetRSSI = r.rssi;
    if (r.rssi > peakRSSI || peakRSSI == NO_SIGNAL) peakRSSI = r.rssi;
    if (strlen(targetName) == 0 && strlen(r.ssid) > 0) {
        strlcpy(targetName, r.ssid, sizeof(targetName));
    }
    HawkPet::feed(FEED_FOXHUNTER, 1);
}

// ── Proximity beeper ──────────────────────────────────────────────────────────
void proximityBeep(int8_t rssi) {
    if (rssi == NO_SIGNAL) return;
    uint16_t freq     = map(constrain(rssi, -100, -20), -100, -20, 400, 2200);
    uint32_t interval = map(constrain(rssi, -100, -20), -100, -20, 2000, 100);
    if (millis() - lastBeepMs > interval) {
        lastBeepMs = millis();
        M5.Speaker.tone(freq, 40);
    }
}

// ── Mode select UI ────────────────────────────────────────────────────────────
bool foxModeSelect() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);

    Display::drawCard(40, 80, Display::width()-80, 400, "Hunt Mode", HH_AMBER);

    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.6f);
    Display::setCursor(60, 130);
    Display::print("What are you hunting?");

    // BLE button
    Display::fillRoundRect(60, 180, Display::width()-120, 80, 16, HH_PURPLE);
    Display::setTextColor(HH_WHITE, HH_PURPLE);
    Display::setTextSize(1.8f);
    Display::setCursor(80, 210);
    Display::print("📶  BLE Device");

    // WiFi button
    Display::fillRoundRect(60, 280, Display::width()-120, 80, 16, HH_TEAL);
    Display::setTextColor(HH_WHITE, HH_TEAL);
    Display::setCursor(80, 310);
    Display::print("📡  WiFi Network");

    // Cancel
    Display::fillRoundRect(60, 380, Display::width()-120, 60, 12, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setTextSize(1.5f);
    Display::setCursor(80, 400);
    Display::print("✕  Cancel");

    while (true) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.y >= 180 && evt.y <= 260) { huntingBLE = true;  return true; }
            if (evt.y >= 280 && evt.y <= 360) { huntingBLE = false; return true; }
            if (evt.y >= 380 && evt.y <= 440) return false;
        }
        delay(30);
    }
}

// ── Main mode ─────────────────────────────────────────────────────────────────
void mode_foxhunter() {
    targetRSSI = NO_SIGNAL;
    peakRSSI   = NO_SIGNAL;
    lastBeepMs = 0;
    memset(targetName, 0, sizeof(targetName));
    memset(targetMAC,  0, sizeof(targetMAC));

    if (!foxModeSelect()) return;

    if (huntingBLE) {
        Comms::onBLEResult(onFoxBLE);
        Comms::startBLEScan();
    } else {
        Comms::onWiFiResult(onFoxWiFi);
        Comms::startWiFiScan();
    }

    renderFoxhunter();
    uint32_t lastRender = millis();

    while (true) {
        M5.update();
        Comms::poll();
        proximityBeep(targetRSSI);

        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed() && evt.x < 30) break;  // swipe left = exit

        if (millis() - lastRender > 250) {
            lastRender = millis();
            renderFoxhunter();
        }

        HawkPet::tick();
        delay(16);
    }

    Comms::stopAll();
    Comms::onBLEResult(nullptr);
    Comms::onWiFiResult(nullptr);
    M5.Speaker.stop();

    if (peakRSSI != NO_SIGNAL) {
        HawkPet::feed(FEED_FOXHUNTER, 2);
        char msg[32];
        snprintf(msg, sizeof(msg), "Peak: %d dBm", peakRSSI);
        Display::showAlert("Hunt Complete", msg, HH_AMBER, 2000);
    }
}
