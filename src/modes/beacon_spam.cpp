// ============================================================
//  HeathenHawk Talon5 — modes/beacon_spam.cpp
//  WiFi beacon spam — proper frame injection via hosted C6 driver
//  Uses esp_wifi_80211_tx() in promiscuous mode on WIFI_IF_STA
//  Based on confirmed working approach for ESP32-P4 + C6 SDIO
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include "../comms/comms_manager.h"
#include <esp_wifi.h>

#define MODE_NAME "Beacon Spam"
#define STATUS_H  64

static const char* FUNNY_SSIDS[] = {
    "FBI Surveillance Van #4",
    "Pretty Fly for a WiFi",
    "HeathenHawk RedTeam",
    "Loading...",
    "Not The FBI",
    "Searching...",
    "Get Your Own WiFi",
    "TellMyWifiLoveHer",
    "HideYoKidsHideYoWifi",
    "Winternet is Coming",
    "The Promised LAN",
    "LAN of Milk and Honey",
    "Series of Tubes",
    "Wu-Tang LAN",
    "DHARMA Initiative",
    "Skynet Global Defense",
    "Area51 TestNetwork",
    "404 Network Unavailable",
    "Router I Hardly Know Her",
    "Bill Wi the Science Fi",
    "Tell My WiFi Love Her",
    "Drop It Like Its Hotspot",
    "The LAN Before Time",
    "Abraham Linksys",
    "Silence of the LANs",
    "John Wilkes Bluetooth",
    "HeathenHawk is Watching",
    "ProTechTor Network",
    "Heavens Heathens WiFi",
    "No Signal Here Move Along",
};
static const uint8_t SSID_COUNT = 30;

// ── Proper beacon frame builder ───────────────────────────────────────────────
// Works with esp_wifi_80211_tx() in promiscuous/STA mode
static uint16_t seqNum = 0;

void injectBeacon(const char* ssid, uint8_t channel) {
    uint8_t frame[256];
    memset(frame, 0, sizeof(frame));
    int p = 0;

    // ── Radiotap header ───────────────────────────────────────────────────────
    frame[p++] = 0x00;  // revision
    frame[p++] = 0x00;  // pad
    frame[p++] = 0x08;  // header length (8 bytes)
    frame[p++] = 0x00;
    frame[p++] = 0x00;  // present flags (none)
    frame[p++] = 0x00;
    frame[p++] = 0x00;
    frame[p++] = 0x00;

    // ── 802.11 beacon frame ───────────────────────────────────────────────────
    // Frame control
    frame[p++] = 0x80;  // type/subtype: beacon
    frame[p++] = 0x00;  // flags

    // Duration
    frame[p++] = 0x00;
    frame[p++] = 0x00;

    // Destination: broadcast
    memset(&frame[p], 0xFF, 6); p += 6;

    // Source MAC: randomized
    uint8_t src[6];
    esp_fill_random(src, 6);
    src[0] = (src[0] & 0xFE) | 0x02;  // locally administered
    memcpy(&frame[p], src, 6); p += 6;

    // BSSID: same as source
    memcpy(&frame[p], src, 6); p += 6;

    // Sequence control
    frame[p++] = (seqNum & 0x0F) << 4;
    frame[p++] = (seqNum >> 4) & 0xFF;
    seqNum = (seqNum + 1) & 0xFFF;

    // ── Beacon body ───────────────────────────────────────────────────────────
    // Timestamp (8 bytes)
    uint64_t ts = esp_timer_get_time();
    memcpy(&frame[p], &ts, 8); p += 8;

    // Beacon interval: 100 TU = 0x0064
    frame[p++] = 0x64;
    frame[p++] = 0x00;

    // Capability info: ESS + ShortPreamble
    frame[p++] = 0x31;
    frame[p++] = 0x04;

    // ── Information Elements ──────────────────────────────────────────────────
    // SSID IE (ID=0)
    uint8_t ssidLen = min((int)strlen(ssid), 32);
    frame[p++] = 0x00;
    frame[p++] = ssidLen;
    memcpy(&frame[p], ssid, ssidLen); p += ssidLen;

    // Supported Rates IE (ID=1)
    frame[p++] = 0x01;
    frame[p++] = 0x08;
    frame[p++] = 0x82;  // 1 Mbps  (basic)
    frame[p++] = 0x84;  // 2 Mbps  (basic)
    frame[p++] = 0x8B;  // 5.5 Mbps (basic)
    frame[p++] = 0x96;  // 11 Mbps (basic)
    frame[p++] = 0x24;  // 18 Mbps
    frame[p++] = 0x30;  // 24 Mbps
    frame[p++] = 0x48;  // 36 Mbps
    frame[p++] = 0x60;  // 48 Mbps

    // DS Parameter Set IE (ID=3) — CRITICAL: sets the channel
    frame[p++] = 0x03;
    frame[p++] = 0x01;
    frame[p++] = channel;

    // Traffic Indication Map IE (ID=5)
    frame[p++] = 0x05;
    frame[p++] = 0x04;
    frame[p++] = 0x00;  // DTIM count
    frame[p++] = 0x01;  // DTIM period
    frame[p++] = 0x00;  // bitmap control
    frame[p++] = 0x00;  // partial virtual bitmap

    // RSN IE (ID=48) — makes it look like WPA2
    frame[p++] = 0x30;
    frame[p++] = 0x14;
    frame[p++] = 0x01; frame[p++] = 0x00;  // version
    frame[p++] = 0x00; frame[p++] = 0x0F; frame[p++] = 0xAC; frame[p++] = 0x04;  // group cipher: CCMP
    frame[p++] = 0x01; frame[p++] = 0x00;  // pairwise count
    frame[p++] = 0x00; frame[p++] = 0x0F; frame[p++] = 0xAC; frame[p++] = 0x04;  // CCMP
    frame[p++] = 0x01; frame[p++] = 0x00;  // AKM count
    frame[p++] = 0x00; frame[p++] = 0x0F; frame[p++] = 0xAC; frame[p++] = 0x02;  // PSK
    frame[p++] = 0x00; frame[p++] = 0x00;  // capabilities

    // Extended Supported Rates IE (ID=50)
    frame[p++] = 0x32;
    frame[p++] = 0x04;
    frame[p++] = 0x0C;  // 6 Mbps
    frame[p++] = 0x12;  // 9 Mbps
    frame[p++] = 0x18;  // 12 Mbps
    frame[p++] = 0x60;  // 48 Mbps

    // Send it — use WIFI_IF_STA for injection
    esp_wifi_80211_tx(WIFI_IF_STA, frame, p, false); // C6 hosted driver — effectiveness varies
}

// ── Mode select ───────────────────────────────────────────────────────────────
int8_t beaconModeSelect() {
    struct { const char* label; uint32_t color; } modes[] = {
        {"Funny SSIDs",   HH_PURPLE},
        {"Random Names",  HH_TEAL},
        {"Custom SSID",   HH_AMBER},
        {"Cancel",        HH_GRAY},
    };
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK);
    Display::setTextSize(3.2f);
    Display::setCursor(40,STATUS_H+20);
    Display::print("Select spam mode:");

    int32_t btnH=120, btnGap=16, startY=STATUS_H+100;
    for (int i=0;i<4;i++) {
        int32_t by=startY+i*(btnH+btnGap);
        Display::fillRoundRect(40,by,Display::width()-80,btnH,20,modes[i].color);
        Display::setTextColor(HH_WHITE,modes[i].color);
        Display::setTextSize(3.5f);
        Display::setCursor(60,by+38);
        Display::print(modes[i].label);
    }
    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            for (int i=0;i<4;i++) {
                int32_t by=startY+i*(btnH+btnGap);
                if (tp[0].y>=by&&tp[0].y<=by+btnH) return i;
            }
        }
        delay(30);
    }
}

// ── Simple custom SSID entry ──────────────────────────────────────────────────
String enterSSID() {
    String ssid="HeathenHawk";
    const char* charset="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !#_-";
    int charsetLen=strlen(charset);
    int8_t charIdx=0;
    int32_t bw=(Display::width()-80)/3;

    auto redraw=[&](){
        Display::clear(HH_DARK);
        Display::drawStatusBar("Custom SSID",false,false,false,100);
        Display::fillRoundRect(20,STATUS_H+20,Display::width()-40,90,12,HH_DARKCARD);
        Display::setTextColor(ssid.length()>0?HH_WHITE:HH_GRAY,HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(36,STATUS_H+44);
        Display::print(ssid.length()>0?ssid.c_str():"...");
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(2.5f);
        Display::setCursor(20,STATUS_H+140);
        Display::print("Current character:");
        Display::setTextColor(HH_AMBER,HH_DARK);
        Display::setTextSize(6.0f);
        char cb[2]={charset[charIdx],0};
        Display::setCursor(Display::width()/2-30,STATUS_H+185);
        Display::print(cb);
        // Row 1: prev / next / add
        Display::fillRoundRect(40,STATUS_H+320,bw,90,12,HH_PURPLE);
        Display::setTextColor(HH_WHITE,HH_PURPLE);
        Display::setTextSize(3.5f);
        Display::setCursor(40+bw/2-22,STATUS_H+344);
        Display::print("<");
        Display::fillRoundRect(40+bw+20,STATUS_H+320,bw,90,12,HH_PURPLE);
        Display::setTextColor(HH_WHITE,HH_PURPLE);
        Display::setCursor(40+bw+20+bw/2-22,STATUS_H+344);
        Display::print(">");
        Display::fillRoundRect(40+2*(bw+20),STATUS_H+320,bw,90,12,HH_GREEN);
        Display::setTextColor(HH_WHITE,HH_GREEN);
        Display::setTextSize(2.8f);
        Display::setCursor(40+2*(bw+20)+bw/2-30,STATUS_H+344);
        Display::print("ADD");
        // Row 2: del / space / done
        Display::fillRoundRect(40,STATUS_H+430,bw,90,12,HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_CORAL);
        Display::setCursor(40+bw/2-28,STATUS_H+454);
        Display::print("DEL");
        Display::fillRoundRect(40+bw+20,STATUS_H+430,bw,90,12,HH_TEAL);
        Display::setTextColor(HH_WHITE,HH_TEAL);
        Display::setCursor(40+bw+20+bw/2-32,STATUS_H+454);
        Display::print("SPC");
        Display::fillRoundRect(40+2*(bw+20),STATUS_H+430,bw,90,12,HH_AMBER);
        Display::setTextColor(HH_WHITE,HH_AMBER);
        Display::setCursor(40+2*(bw+20)+bw/2-38,STATUS_H+454);
        Display::print("DONE");
    };

    redraw();
    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            int32_t tx=tp[0].x, ty=tp[0].y;
            int32_t r1=STATUS_H+320, r2=STATUS_H+430;
            if (ty>=r1&&ty<=r1+90) {
                if (tx<40+bw) charIdx=(charIdx-1+charsetLen)%charsetLen;
                else if (tx<40+2*bw+20) charIdx=(charIdx+1)%charsetLen;
                else if (ssid.length()<32) ssid+=charset[charIdx];
            } else if (ty>=r2&&ty<=r2+90) {
                if (tx<40+bw) { if (ssid.length()>0) ssid.remove(ssid.length()-1); }
                else if (tx<40+2*bw+20) { if (ssid.length()<32) ssid+=' '; }
                else return ssid.length()>0?ssid:String("HeathenHawk");
            }
            redraw();
        }
        delay(30);
    }
}

// ── Main spam loop ────────────────────────────────────────────────────────────
void runBeaconSpam(int8_t modeIdx, const String& customSSID) {
    // Initialize WiFi in STA mode, then enable promiscuous for raw TX
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Enable promiscuous mode — required for esp_wifi_80211_tx to work
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    uint32_t framesSent=0;
    uint8_t  ssidIdx=0;
    uint8_t  channel=1;
    uint32_t lastRender=0;

    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);

    while (true) {
        M5.update();

        // Pick SSID
        const char* ssid;
        char randSSID[33];
        if (modeIdx==0) {
            ssid=FUNNY_SSIDS[ssidIdx%SSID_COUNT];
        } else if (modeIdx==2) {
            ssid=customSSID.c_str();
        } else {
            snprintf(randSSID,sizeof(randSSID),"Network_%04X_%02X",
                     (int)random(0xFFFF),(int)random(0xFF));
            ssid=randSSID;
        }

        injectBeacon(ssid, channel);
        framesSent++;
        ssidIdx++;

        // Channel hop every 15 beacons when using funny/random
        if (modeIdx!=2 && framesSent%15==0) {
            channel=(channel%13)+1;
            esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        }

        if (framesSent%100==0) HawkPet::feed(FEED_BEACON_SPAM,1);

        if (millis()-lastRender>700) {
            lastRender=millis();
            Display::fillRect(0,STATUS_H,Display::width(),
                              Display::height()-STATUS_H,HH_DARK);
            Display::setTextColor(HH_AMBER,HH_DARK);
            Display::setTextSize(9.0f);
            Display::setCursor(40,STATUS_H+40);
            char buf[16]; snprintf(buf,sizeof(buf),"%lu",framesSent);
            Display::print(buf);
            Display::setTextColor(HH_GRAY,HH_DARK);
            Display::setTextSize(3.0f);
            Display::setCursor(40,STATUS_H+220);
            Display::print("beacons injected");
            Display::setTextSize(2.5f);
            Display::setCursor(40,STATUS_H+290);
            Display::printf("Ch: %d", channel);
            Display::setCursor(40,STATUS_H+345);
            char trunc[36]; strlcpy(trunc,ssid,sizeof(trunc));
            Display::printf("SSID: %.35s",trunc);

            Display::fillRoundRect(40,Display::height()-130,
                                   Display::width()-80,100,20,HH_GRAY);
            Display::setTextColor(HH_WHITE,HH_GRAY);
            Display::setTextSize(3.5f);
            Display::setCursor(80,Display::height()-98);
            Display::print("STOP");
        }

        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-140) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            break;
        }

        delay(50);
    }

    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
    char msg[32]; snprintf(msg,sizeof(msg),"%lu beacons injected",framesSent);
    Display::showAlert("Stopped",msg,HH_GRAY,2000);
}

void mode_beacon_spam() {
    int8_t modeIdx=beaconModeSelect();
    if (modeIdx==3) return;
    String customSSID="HeathenHawk";
    if (modeIdx==2) customSSID=enterSSID();
    runBeaconSpam(modeIdx,customSSID);
}
