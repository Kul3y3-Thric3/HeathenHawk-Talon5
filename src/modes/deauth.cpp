// ============================================================
//  HeathenHawk Talon5 — modes/deauth.cpp
//  WiFi deauthentication via C6 co-processor
//  Full touch UI — scan, select, attack
//  Authorized use only.
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>

#define MODE_NAME "Deauth"
#define MAX_NETS  50

struct DeauthNet { char ssid[33]; char bssid[18]; int8_t rssi; int channel; };
static DeauthNet nets[MAX_NETS];
static int16_t   netCount   = 0;
static int16_t   selectedIdx = 0;
static bool      attacking  = false;
static bool      scanning   = false;
static uint32_t  framesSent = 0;
static bool      needRedraw = true;

void onDeauthNet(const WiFiResult& r) {
    for (int i = 0; i < netCount; i++)
        if (strcasecmp(nets[i].bssid, r.bssid) == 0) { nets[i].rssi = r.rssi; return; }
    if (netCount >= MAX_NETS) return;
    strlcpy(nets[netCount].ssid,  r.ssid,  sizeof(nets[0].ssid));
    strlcpy(nets[netCount].bssid, r.bssid, sizeof(nets[0].bssid));
    nets[netCount].rssi    = r.rssi;
    nets[netCount].channel = r.channel;
    netCount++;
    needRedraw = true;
}

void renderConsent() {
    Display::clear(HH_DARK);
    Display::drawCard(40, 60, Display::width()-80, 500, "!! Legal Warning !!", HH_RED);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.5f);
    Display::setCursor(60, 120);
    Display::print("Deauth attacks are ILLEGAL");
    Display::setCursor(60, 155);
    Display::print("without written permission.");
    Display::setCursor(60, 190);
    Display::print("Only use on networks you");
    Display::setCursor(60, 225);
    Display::print("own or have explicit written");
    Display::setCursor(60, 260);
    Display::print("authorization to test.");

    Display::fillRoundRect(60, 330, Display::width()-120, 70, 16, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(1.6f);
    Display::setCursor(80, 355);
    Display::print("I have permission — Continue");

    Display::fillRoundRect(60, 420, Display::width()-120, 60, 16, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setCursor(80, 440);
    Display::print("Cancel");
}

void renderNetList() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);
    Display::fillRect(0, 48, Display::width(), 14, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(1.0f);
    Display::setCursor(16, 52);
    Display::print("AUTHORIZED USE ONLY");

    Display::fillRect(0, 62, Display::width(), 30, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 70);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d networks found", netCount);
    Display::print(hdr);

    uint32_t scanCol = scanning ? HH_CORAL : HH_GREEN;
    Display::fillRoundRect(Display::width()-140, 66, 124, 22, 6, scanCol);
    Display::setTextColor(HH_WHITE, scanCol);
    Display::setCursor(Display::width()-128, 72);
    Display::print(scanning ? "● Scanning" : "▶ Scan");

    if (netCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.5f);
        Display::setCursor(40, Display::height()/2);
        Display::print("Tap Scan to find networks");
        return;
    }

    int32_t y = 100;
    int32_t rh = 60;
    for (int16_t i = 0; i < netCount && y < Display::height()-60; i++) {
        bool sel = (i == selectedIdx);
        Display::fillRect(0, y, Display::width(), rh, sel ? HH_RED : HH_DARKCARD);
        Display::drawLine(0, y+rh-1, Display::width(), y+rh-1, HH_DARK);
        Display::setTextColor(HH_WHITE, sel ? HH_RED : HH_DARKCARD);
        Display::setTextSize(1.4f);
        Display::setCursor(16, y+10);
        Display::print(strlen(nets[i].ssid) > 0 ? nets[i].ssid : "[Hidden]");
        Display::setTextColor(sel ? HH_WHITE : HH_GRAY, sel ? HH_RED : HH_DARKCARD);
        Display::setTextSize(1.2f);
        Display::setCursor(16, y+34);
        char det[48]; snprintf(det,sizeof(det),"Ch%d  %ddBm  %s",
                               nets[i].channel, nets[i].rssi, nets[i].bssid);
        Display::print(det);
        y += rh;
    }

    // Attack button
    Display::fillRoundRect(40, Display::height()-80, Display::width()-80, 60, 16, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(1.8f);
    Display::setCursor(60, Display::height()-58);
    Display::print("⚡  Deauth Selected");
    needRedraw = false;
}

void renderAttack() {
    Display::clear(HH_DARK);
    Display::drawStatusBar("ATTACKING", false, false, false, 100);
    Display::fillRect(0, 48, Display::width(), 20, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(1.1f);
    Display::setCursor(16, 54);
    Display::print("!! AUTHORIZED USE ONLY !!");

    Display::setTextColor(HH_RED, HH_DARK);
    Display::setTextSize(4.0f);
    Display::setCursor(40, 90);
    Display::print("DEAUTH");

    DeauthNet& t = nets[selectedIdx];
    Display::setTextColor(HH_WHITE, HH_DARK);
    Display::setTextSize(1.5f);
    Display::setCursor(16, 200);
    Display::printf("Target: %s", t.ssid[0] ? t.ssid : "[Hidden]");
    Display::setCursor(16, 235);
    Display::printf("BSSID:  %s", t.bssid);
    Display::setCursor(16, 270);
    Display::printf("Ch: %d", t.channel);

    Display::setTextColor(HH_AMBER, HH_DARK);
    Display::setTextSize(3.0f);
    Display::setCursor(16, 320);
    char buf[24]; snprintf(buf, sizeof(buf), "%lu", framesSent);
    Display::print(buf);
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 370);
    Display::print("frames sent");

    Display::fillRoundRect(40, Display::height()-100, Display::width()-80, 70, 16, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setTextSize(1.8f);
    Display::setCursor(60, Display::height()-72);
    Display::print("■  Stop Attack");
}

void mode_deauth() {
    netCount = 0; selectedIdx = 0; attacking = false;
    scanning = false; framesSent = 0; needRedraw = true;
    memset(nets, 0, sizeof(nets));

    // Consent screen
    renderConsent();
    while (true) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.y >= 330 && evt.y <= 400) break;
            if (evt.y >= 420) return;
        }
        delay(30);
    }

    Comms::onWiFiResult(onDeauthNet);
    renderNetList();

    while (!attacking) {
        M5.update(); Comms::poll();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) { Comms::onWiFiResult(nullptr); return; }
            // Scan button
            if (evt.y >= 66 && evt.y <= 92 && evt.x >= Display::width()-140) {
                scanning = !scanning;
                if (scanning) Comms::startWiFiScan();
                else Comms::stopAll();
                needRedraw = true;
            }
            // Network rows
            else if (evt.y >= 100 && evt.y < Display::height()-80) {
                int16_t idx = (evt.y - 100) / 60;
                if (idx < netCount) { selectedIdx = idx; needRedraw = true; }
            }
            // Attack button
            else if (evt.y >= Display::height()-80) {
                if (netCount > 0) attacking = true;
            }
        }
        if (needRedraw) renderNetList();
        HawkPet::tick(); delay(16);
    }

    // Attack loop
    Comms::stopAll();
    Comms::onWiFiResult(nullptr);
    Comms::startDeauth(nets[selectedIdx].bssid, nets[selectedIdx].channel);
    renderAttack();

    while (attacking) {
        M5.update(); framesSent += 2;
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed() && evt.y >= Display::height()-100) { attacking = false; break; }
        if (framesSent % 50 == 0) { HawkPet::feed(FEED_DEAUTH, 1); renderAttack(); }
        delay(10);
    }

    Comms::stopAll();
    char msg[32]; snprintf(msg, sizeof(msg), "%lu frames sent", framesSent);
    Display::showAlert("Stopped", msg, HH_GRAY, 2000);
}
