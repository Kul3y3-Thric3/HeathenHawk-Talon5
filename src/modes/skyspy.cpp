// ============================================================
//  HeathenHawk Talon5 — modes/skyspy.cpp
//  FAA Remote ID drone detection — non-blocking BLE scan
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

#define MODE_NAME  "Sky Spy"
#define LOG_FILE   "/skyspy_log.csv"
#define MAX_DRONES 30
#define STATUS_H   64
#define SCAN_SECS  5

struct DroneRec {
    char     id[24];
    int8_t   rssi;
    uint32_t firstSeenMs;
    uint32_t lastSeenMs;
    uint32_t frameCount;
};

static DroneRec drones[MAX_DRONES];
static uint8_t  droneCount  = 0;
static bool     sdReady     = false;
static bool     needRedraw  = true;
static bool     scanning    = false;

int8_t findOrAddDrone(const char* id) {
    for (uint8_t i=0; i<droneCount; i++)
        if (strcasecmp(drones[i].id,id)==0) return i;
    if (droneCount>=MAX_DRONES) return -1;
    int8_t idx=droneCount++;
    memset(&drones[idx],0,sizeof(DroneRec));
    strlcpy(drones[idx].id,id,sizeof(drones[idx].id));
    drones[idx].firstSeenMs=millis();
    return idx;
}

class SkyScanCB : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice adv) {
        // Check for Remote ID UUID 0xFFFA
        bool isRID=false;
        if (adv.haveServiceUUID()) {
            BLEUUID ridUUID((uint16_t)0xFFFA);
            for (int i=0;i<(int)adv.getServiceUUIDCount();i++) {
                if (adv.getServiceUUID(i).equals(ridUUID)) { isRID=true; break; }
            }
        }
        if (!isRID) return;
        char mac[18];
        strlcpy(mac,adv.getAddress().toString().c_str(),sizeof(mac));
        int8_t idx=findOrAddDrone(mac);
        if (idx<0) return;
        bool isNew=(drones[idx].lastSeenMs==0);
        drones[idx].rssi=adv.getRSSI();
        drones[idx].lastSeenMs=millis();
        drones[idx].frameCount++;
        if (isNew) {
            M5.Speaker.tone(1200,100);delay(130);
            M5.Speaker.tone(1600,100);delay(130);
            M5.Speaker.tone(2000,150);
            Display::showAlert("DRONE DETECTED!",mac,HH_BLUE,1500);
            HawkPet::feed(FEED_SKYSPY,1);
            if (sdReady) {
                File f=SD.open(LOG_FILE,FILE_APPEND);
                if (f){f.printf("%s,%d,%lu\n",mac,drones[idx].rssi,millis());f.close();}
            }
        }
        needRedraw=true;
    }
};
static SkyScanCB skyCB;

void renderSky2() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);
    Display::fillRect(0,STATUS_H,Display::width(),120,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.5f);
    Display::setCursor(24,STATUS_H+26);
    char hdr[32];
    snprintf(hdr,sizeof(hdr),"%d drone%s detected",droneCount,droneCount==1?"":"s");
    Display::print(hdr);

    uint32_t btnCol=scanning?HH_CORAL:HH_BLUE;
    Display::fillRoundRect(Display::width()-320,STATUS_H+20,300,80,16,btnCol);
    Display::setTextColor(HH_WHITE,btnCol);
    Display::setTextSize(3.0f);
    Display::setCursor(Display::width()-300,STATUS_H+40);
    Display::print(scanning?"  SCANNING":"  START");

    // Back button always visible
    Display::fillRoundRect(20,Display::height()-110,200,80,16,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90);
    Display::print("BACK");

    if (droneCount==0) {
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()/2-60);
        Display::print("Listening for FAA Remote ID...");
        Display::setTextSize(2.5f);
        Display::setCursor(40,Display::height()/2);
        Display::print("ASTM F3411  BLE  Passive only");
        needRedraw=false; return;
    }
    int32_t y=STATUS_H+120,rh=110;
    for (uint8_t i=0;i<droneCount&&y<Display::height()-130;i++) {
        DroneRec& d=drones[i];
        bool active=(millis()-d.lastSeenMs<15000);
        uint32_t col=active?HH_BLUE:HH_GRAY;
        char detail[64];
        snprintf(detail,sizeof(detail),"BLE  x%lu frames  %d dBm",d.frameCount,d.rssi);
        Display::drawScanRow(y,rh,d.id,detail,"DRONE",d.rssi,false,col);
        y+=rh;
    }
    needRedraw=false;
}

void mode_skyspy() {
    droneCount=0;scanning=false;needRedraw=true;
    memset(drones,0,sizeof(drones));
    sdReady=SD.begin();

    BLEDevice::init("HH");
    BLEScan* pScan=BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&skyCB);
    pScan->setActiveScan(false);
    pScan->setInterval(100);
    pScan->setWindow(90);

    renderSky2();
    uint32_t scanStartMs=0;

    while (true) {
        M5.update();

        // Non-blocking scan
        if (scanning && !pScan->isScanning() && millis()-scanStartMs>500) {
            pScan->start(SCAN_SECS,false);
            scanStartMs=millis();
        }

        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);

        if (tapped) {
            // Back button
            if (tp[0].y>=Display::height()-120 && tp[0].x<240) break;
            if (tp[0].x<60) break;

            // Start/stop button
            if (tp[0].y>=STATUS_H+10 && tp[0].y<=STATUS_H+110 &&
                tp[0].x>=Display::width()-340) {
                scanning=!scanning;
                if (!scanning) pScan->stop();
                else scanStartMs=0;
                needRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        if (needRedraw) renderSky2();
        HawkPet::tick();
        delay(20);
    }

    pScan->stop();
    BLEDevice::deinit();
}
