// ============================================================
//  HeathenHawk Talon5 — modes/flockyou.cpp
//  Surveillance camera detection — Flock Safety, Raven, Rekor, Axon
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#define MODE_NAME "Flock-You"
#define LOG_FILE  "/flockyou_log.csv"
#define MAX_CAMS  50

struct CamRecord {
    char     mac[18];
    char     ssid[33];
    char     vendor[24];
    char     type[24];
    int8_t   rssi;
    uint8_t  source;
    uint32_t lastSeenMs;
    uint32_t alertColor;
};

static CamRecord cams[MAX_CAMS];
static uint8_t   camCount   = 0;
static bool      sdReady    = false;
static bool      needRedraw = true;
static bool      scanning   = false;

struct CamSig { const char* oui; const char* ssidFrag; const char* vendor; const char* type; uint32_t color; };
static const CamSig SIGS[] = {
    {"00:0C:E7", nullptr, "Flock Safety", "ALPR Camera",  HH_CORAL},
    {"AC:67:B2", nullptr, "Flock Safety", "ALPR Camera",  HH_CORAL},
    {"70:03:9F", nullptr, "Raven",        "ALPR Camera",  HH_CORAL},
    {"B4:E6:2A", nullptr, "Rekor",        "Scout ALPR",   HH_AMBER},
    {"D8:96:E0", nullptr, "Axon",         "Evidence Cam", HH_AMBER},
    {nullptr, "flock",    "Flock Safety", "ALPR (SSID)",  HH_CORAL},
    {nullptr, "ALPR",     "Unknown",      "ALPR Camera",  HH_AMBER},
    {nullptr, "axon-",    "Axon",         "Evidence Cam", HH_AMBER},
};
static const uint8_t SIG_COUNT = sizeof(SIGS)/sizeof(SIGS[0]);

const CamSig* matchOUI(const char* mac) {
    for (uint8_t i = 0; i < SIG_COUNT; i++)
        if (SIGS[i].oui && strncasecmp(mac, SIGS[i].oui, 8) == 0) return &SIGS[i];
    return nullptr;
}
const CamSig* matchSSID(const char* ssid) {
    for (uint8_t i = 0; i < SIG_COUNT; i++)
        if (SIGS[i].ssidFrag && strcasestr(ssid, SIGS[i].ssidFrag)) return &SIGS[i];
    return nullptr;
}

int8_t findOrAddCam(const char* mac) {
    for (uint8_t i = 0; i < camCount; i++)
        if (strcasecmp(cams[i].mac, mac) == 0) return i;
    if (camCount >= MAX_CAMS) return -1;
    int8_t idx = camCount++;
    memset(&cams[idx], 0, sizeof(CamRecord));
    strlcpy(cams[idx].mac, mac, sizeof(cams[idx].mac));
    return idx;
}

void onFlockBLE(const BLEResult& r) {
    const CamSig* sig = matchOUI(r.mac);
    if (!sig) return;
    int8_t idx = findOrAddCam(r.mac);
    if (idx < 0) return;
    bool isNew = (cams[idx].lastSeenMs == 0);
    cams[idx].rssi = r.rssi; cams[idx].lastSeenMs = millis();
    cams[idx].source = 0; cams[idx].alertColor = sig->color;
    strlcpy(cams[idx].vendor, sig->vendor, sizeof(cams[idx].vendor));
    strlcpy(cams[idx].type,   sig->type,   sizeof(cams[idx].type));
    if (isNew) {
        for (int i=0;i<3;i++){M5.Speaker.tone(1047,80);delay(120);}
        Display::showAlert(sig->vendor, sig->type, sig->color, 1800);
        HawkPet::feed(FEED_FLOCKYOU, 1);
        needRedraw = true;
    }
}

void onFlockWiFi(const WiFiResult& r) {
    const CamSig* sig = matchOUI(r.bssid);
    uint8_t src = 1;
    if (!sig && strlen(r.ssid)>0) { sig = matchSSID(r.ssid); src=2; }
    if (!sig) return;
    int8_t idx = findOrAddCam(r.bssid);
    if (idx < 0) return;
    bool isNew = (cams[idx].lastSeenMs == 0);
    cams[idx].rssi = r.rssi; cams[idx].lastSeenMs = millis();
    cams[idx].source = src; cams[idx].alertColor = sig->color;
    strlcpy(cams[idx].vendor, sig->vendor, sizeof(cams[idx].vendor));
    strlcpy(cams[idx].type,   sig->type,   sizeof(cams[idx].type));
    strlcpy(cams[idx].ssid,   r.ssid,      sizeof(cams[idx].ssid));
    if (isNew) {
        for (int i=0;i<3;i++){M5.Speaker.tone(1047,80);delay(120);}
        Display::showAlert(sig->vendor, sig->type, sig->color, 1800);
        HawkPet::feed(FEED_FLOCKYOU, 1);
        needRedraw = true;
    }
}

void renderFlock() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, false, 100);
    Display::fillRect(0, 48, Display::width(), 40, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 58);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "%d surveillance device%s", camCount, camCount==1?"":"s");
    Display::print(hdr);

    uint32_t btnCol = scanning ? HH_CORAL : HH_GREEN;
    Display::fillRoundRect(Display::width()-140, 54, 124, 28, 8, btnCol);
    Display::setTextColor(HH_WHITE, btnCol);
    Display::setCursor(Display::width()-128, 62);
    Display::print(scanning ? "● Active" : "▶ Start");

    if (camCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.5f);
        Display::setCursor(40, Display::height()/2 - 40);
        Display::print("Monitoring for ALPR cameras...");
        Display::setTextSize(1.3f);
        Display::setCursor(40, Display::height()/2 + 10);
        Display::print("Flock Safety  •  Raven  •  Rekor  •  Axon");
        return;
    }

    int32_t y = 96;
    int32_t rh = 72;
    for (uint8_t i = 0; i < camCount && (i < (Display::height()-96)/rh); i++) {
        CamRecord& c = cams[i];
        bool active = (millis() - c.lastSeenMs < 30000);
        uint32_t col = active ? c.alertColor : HH_GRAY;
        const char* src[] = {"BLE","WiFi","SSID"};
        char detail[64];
        snprintf(detail, sizeof(detail), "%s  •  %s  •  %d dBm",
                 c.mac, src[min((int)c.source,2)], c.rssi);
        Display::drawScanRow(y, rh, c.vendor, detail, c.type, c.rssi, false, col);
        y += rh;
    }
    needRedraw = false;
}

void mode_flockyou() {
    camCount = 0; scanning = false; needRedraw = true;
    memset(cams, 0, sizeof(cams));
    sdReady = SD.begin();
    Comms::onBLEResult(onFlockBLE);
    Comms::onWiFiResult(onFlockWiFi);
    renderFlock();

    while (true) {
        M5.update(); Comms::poll();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;
            if (evt.y >= 54 && evt.y <= 82 && evt.x >= Display::width()-140) {
                scanning = !scanning;
                if (scanning) { Comms::startBLEScan(); Comms::startWiFiScan(); }
                else Comms::stopAll();
                needRedraw = true;
            }
        }
        if (needRedraw) renderFlock();
        HawkPet::tick(); delay(16);
    }
    Comms::stopAll();
    Comms::onBLEResult(nullptr);
    Comms::onWiFiResult(nullptr);
}
