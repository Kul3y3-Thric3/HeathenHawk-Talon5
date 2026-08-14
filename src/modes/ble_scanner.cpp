// ============================================================
//  HeathenHawk Talon5 — modes/ble_scanner.cpp
//  Fixed: non-blocking BLE scan, proper back button
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <SD.h>

#define MODE_NAME   "BLE Scanner"
#define LOG_FILE    "/ble_scan.csv"
#define MAX_DEVICES 200
#define ROW_H       110
#define HEADER_H    120
#define STATUS_H    64
#define SCAN_SECS   5   // scan duration in seconds — non-blocking

struct BLEDev {
    char     mac[18];
    char     name[32];
    char     type[24];
    int8_t   rssi;
    uint32_t lastSeenMs;
    uint32_t seenCount;
    bool     onWatchlist;
};

static BLEDev   devices[MAX_DEVICES];
static uint16_t devCount    = 0;
static int16_t  scrollIdx   = 0;
static bool     sdReady     = false;
static bool     scanning    = false;
static bool     needsRedraw = true;

static const char* WATCHLIST_OUI[] = {
    "00:0C:E7","AC:67:B2","E4:AA:EC",
    "70:03:9F","B4:E6:2A","D8:96:E0",
};

bool isOnWatchlist(const char* mac) {
    for (uint8_t i=0; i<6; i++)
        if (strncasecmp(mac, WATCHLIST_OUI[i], 8)==0) return true;
    return false;
}

int16_t findOrAdd(const char* mac) {
    for (uint16_t i=0; i<devCount; i++)
        if (strcasecmp(devices[i].mac, mac)==0) return i;
    if (devCount >= MAX_DEVICES) return -1;
    int16_t idx = devCount++;
    memset(&devices[idx], 0, sizeof(BLEDev));
    strlcpy(devices[idx].mac,  mac,           sizeof(devices[idx].mac));
    strlcpy(devices[idx].name, "Unknown",     sizeof(devices[idx].name));
    strlcpy(devices[idx].type, "Generic BLE", sizeof(devices[idx].type));
    return idx;
}

class HHScanCB : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice adv) {
        char mac[18];
        strlcpy(mac, adv.getAddress().toString().c_str(), sizeof(mac));
        int16_t idx = findOrAdd(mac);
        if (idx < 0) return;
        bool isNew = (devices[idx].seenCount == 0);
        if (adv.haveName())
            strlcpy(devices[idx].name, adv.getName().c_str(), sizeof(devices[idx].name));
        if (adv.haveManufacturerData()) {
            String mfr = adv.getManufacturerData();
            if (mfr.length()>=2 && (uint8_t)mfr[0]==0x4C && (uint8_t)mfr[1]==0x00)
                strlcpy(devices[idx].type, "Apple Device", sizeof(devices[idx].type));
        }
        devices[idx].rssi       = adv.getRSSI();
        devices[idx].lastSeenMs = millis();
        devices[idx].seenCount++;
        devices[idx].onWatchlist = isOnWatchlist(mac);
        if (isNew) {
            HawkPet::feed(FEED_BLE_SCAN, 1);
            if (devices[idx].onWatchlist) {
                M5.Speaker.tone(1200,80); delay(100);
                M5.Speaker.tone(2000,100);
                Display::showAlert("WATCHLIST HIT!", mac, HH_CORAL, 1500);
            }
        }
        needsRedraw = true;
    }
};
static HHScanCB scanCB;

static int32_t visibleRows2() {
    return (Display::height()-STATUS_H-HEADER_H)/ROW_H;
}

void renderBLEList2() {
    Display::fillRect(0, STATUS_H, Display::width(),
                      Display::height()-STATUS_H, HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, false, 100);
    Display::fillRect(0, STATUS_H, Display::width(), HEADER_H, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(3.5f);
    Display::setCursor(24, STATUS_H+26);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d BLE devices",devCount);
    Display::print(hdr);

    uint32_t col = scanning ? HH_CORAL : HH_PURPLE;
    Display::fillRoundRect(Display::width()-320, STATUS_H+20, 300, 80, 16, col);
    Display::setTextColor(HH_WHITE, col);
    Display::setTextSize(3.0f);
    Display::setCursor(Display::width()-300, STATUS_H+40);
    Display::print(scanning ? "  SCANNING" : "  START SCAN");

    if (devCount==0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(3.5f);
        Display::setCursor(40, Display::height()/2);
        Display::print("Tap START SCAN to begin");
        needsRedraw=false; return;
    }
    int32_t y=STATUS_H+HEADER_H;
    for (int16_t i=scrollIdx; i<(int16_t)devCount && (i-scrollIdx)<visibleRows2(); i++) {
        BLEDev& d=devices[i];
        bool fresh=(millis()-d.lastSeenMs<15000);
        uint32_t c=d.onWatchlist?HH_CORAL:fresh?HH_PURPLE:HH_GRAY;
        char detail[64];
        snprintf(detail,sizeof(detail),"%s  %s  x%lu",d.mac,d.type,d.seenCount);
        Display::drawScanRow(y,ROW_H,
            strcmp(d.name,"Unknown")!=0?d.name:d.mac,
            detail,d.onWatchlist?"WATCH":d.type,d.rssi,false,c);
        y+=ROW_H;
    }
    needsRedraw=false;
}

void mode_ble_scanner() {
    devCount=0; scrollIdx=0; scanning=false; needsRedraw=true;
    memset(devices,0,sizeof(devices));
    sdReady=SD.begin();

    BLEDevice::init("HH");
    BLEScan* pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&scanCB);
    pScan->setActiveScan(false);
    pScan->setInterval(100);
    pScan->setWindow(90);

    renderBLEList2();
    uint32_t scanStartMs = 0;

    while (true) {
        M5.update();

        // Non-blocking: restart scan every SCAN_SECS seconds
        if (scanning) {
            if (!pScan->isScanning()) {
                if (millis()-scanStartMs > 1000) {
                    pScan->start(SCAN_SECS, false);
                    scanStartMs = millis();
                    needsRedraw=true;
                }
            }
        }

        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);

        if (tapped) {
            // BACK — left 60px strip
            if (tp[0].x < 60) break;

            // Scan button
            if (tp[0].y>=STATUS_H+10 && tp[0].y<=STATUS_H+110 &&
                tp[0].x>=Display::width()-340) {
                scanning=!scanning;
                if (!scanning) pScan->stop();
                else { scanStartMs=0; }
                needsRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        // Scroll
        static int32_t lastTY=0;
        if (num>0 && tp[0].y>STATUS_H+HEADER_H) {
            if (lastTY>0) {
                int32_t dy=lastTY-tp[0].y;
                if (abs(dy)>30) {
                    scrollIdx=constrain(scrollIdx+(dy>0?1:-1),
                        0,max(0,(int)devCount-(int)visibleRows2()));
                    needsRedraw=true; lastTY=tp[0].y;
                }
            } else lastTY=tp[0].y;
        } else lastTY=0;

        if (needsRedraw) renderBLEList2();
        HawkPet::tick();
        delay(20);
    }

    pScan->stop();
    BLEDevice::deinit();
}
