// ============================================================
//  HeathenHawk Talon5 — modes/flockyou.cpp
//  Surveillance camera detection — non-blocking, explicit back
//  Credit: OUI detection approach by @colonelpanichacks (OUI Spy)
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <SD.h>

#define MODE_NAME "Flock-You"
#define LOG_FILE  "/flockyou_log.csv"
#define MAX_CAMS  50
#define STATUS_H  64
#define SCAN_SECS 5

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

struct CamSig {
    const char* oui; const char* ssidFrag;
    const char* vendor; const char* type; uint32_t color;
};
static const CamSig SIGS[] = {
    {"00:0C:E7",nullptr,"Flock Safety","ALPR Camera",HH_CORAL},
    {"AC:67:B2",nullptr,"Flock Safety","ALPR Camera",HH_CORAL},
    {"E4:AA:EC",nullptr,"Flock Safety","ALPR Camera",HH_CORAL},
    {"70:03:9F",nullptr,"Raven","ALPR Camera",HH_CORAL},
    {"B4:E6:2A",nullptr,"Rekor","Scout ALPR",HH_AMBER},
    {"D8:96:E0",nullptr,"Axon","Evidence Cam",HH_AMBER},
    {nullptr,"flock","Flock Safety","ALPR (SSID)",HH_CORAL},
    {nullptr,"ALPR","Unknown","ALPR Camera",HH_AMBER},
    {nullptr,"axon-","Axon","Evidence Cam",HH_AMBER},
};
static const uint8_t SIG_COUNT=sizeof(SIGS)/sizeof(SIGS[0]);

const CamSig* matchOUI(const char* mac) {
    for (uint8_t i=0;i<SIG_COUNT;i++)
        if (SIGS[i].oui && strncasecmp(mac,SIGS[i].oui,8)==0) return &SIGS[i];
    return nullptr;
}
const CamSig* matchSSID(const char* ssid) {
    for (uint8_t i=0;i<SIG_COUNT;i++)
        if (SIGS[i].ssidFrag && strcasestr(ssid,SIGS[i].ssidFrag)) return &SIGS[i];
    return nullptr;
}
int8_t findOrAddCam(const char* mac) {
    for (uint8_t i=0;i<camCount;i++)
        if (strcasecmp(cams[i].mac,mac)==0) return i;
    if (camCount>=MAX_CAMS) return -1;
    int8_t idx=camCount++;
    memset(&cams[idx],0,sizeof(CamRecord));
    strlcpy(cams[idx].mac,mac,sizeof(cams[idx].mac));
    return idx;
}
void camAlert(const CamRecord& c) {
    for (int i=0;i<3;i++){M5.Speaker.tone(1047,80);delay(120);}
    Display::showAlert(c.vendor,c.type,c.alertColor,1800);
    HawkPet::feed(FEED_FLOCKYOU,1);
    needRedraw=true;
}

class FlockBLECB : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice adv) {
        char mac[18];
        strlcpy(mac,adv.getAddress().toString().c_str(),sizeof(mac));
        const CamSig* sig=matchOUI(mac);
        if (!sig) return;
        int8_t idx=findOrAddCam(mac);
        if (idx<0) return;
        bool isNew=(cams[idx].lastSeenMs==0);
        cams[idx].rssi=adv.getRSSI();
        cams[idx].lastSeenMs=millis();
        cams[idx].source=0;
        cams[idx].alertColor=sig->color;
        strlcpy(cams[idx].vendor,sig->vendor,sizeof(cams[idx].vendor));
        strlcpy(cams[idx].type,sig->type,sizeof(cams[idx].type));
        if (isNew) camAlert(cams[idx]);
    }
};
static FlockBLECB flockCB;

void doWiFiFlock() {
    WiFi.mode(WIFI_STA);
    int n=WiFi.scanNetworks(false,true);
    for (int i=0;i<n;i++) {
        uint8_t* raw=WiFi.BSSID(i);
        char bssid[18];
        snprintf(bssid,sizeof(bssid),"%02X:%02X:%02X:%02X:%02X:%02X",
                 raw[0],raw[1],raw[2],raw[3],raw[4],raw[5]);
        const CamSig* sig=matchOUI(bssid);
        uint8_t src=1;
        String ssid=WiFi.SSID(i);
        if (!sig){sig=matchSSID(ssid.c_str());src=2;}
        if (!sig) continue;
        int8_t idx=findOrAddCam(bssid);
        if (idx<0) continue;
        bool isNew=(cams[idx].lastSeenMs==0);
        cams[idx].rssi=WiFi.RSSI(i);
        cams[idx].lastSeenMs=millis();
        cams[idx].source=src;
        cams[idx].alertColor=sig->color;
        strlcpy(cams[idx].vendor,sig->vendor,sizeof(cams[idx].vendor));
        strlcpy(cams[idx].type,sig->type,sizeof(cams[idx].type));
        strlcpy(cams[idx].ssid,ssid.c_str(),sizeof(cams[idx].ssid));
        if (isNew) camAlert(cams[idx]);
    }
    WiFi.scanDelete();
    needRedraw=true;
}

void renderFlock2() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);
    Display::fillRect(0,STATUS_H,Display::width(),120,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.5f);
    Display::setCursor(24,STATUS_H+26);
    char hdr[48];
    snprintf(hdr,sizeof(hdr),"%d surveillance device%s",camCount,camCount==1?"":"s");
    Display::print(hdr);

    uint32_t btnCol=scanning?HH_CORAL:HH_GREEN;
    Display::fillRoundRect(Display::width()-320,STATUS_H+20,300,80,16,btnCol);
    Display::setTextColor(HH_WHITE,btnCol);
    Display::setTextSize(3.0f);
    Display::setCursor(Display::width()-300,STATUS_H+40);
    Display::print(scanning?"  STOP":"  START");

    // Explicit back button
    Display::fillRoundRect(20,Display::height()-110,200,80,16,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90);
    Display::print("BACK");

    if (camCount==0) {
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()/2-60);
        Display::print("Monitoring for:");
        Display::setTextSize(2.5f);
        Display::setCursor(40,Display::height()/2);
        Display::print("Flock Safety  Raven  Rekor  Axon");
        needRedraw=false; return;
    }
    int32_t y=STATUS_H+120,rh=110;
    for (uint8_t i=0;i<camCount&&y<Display::height()-130;i++) {
        CamRecord& c=cams[i];
        bool active=(millis()-c.lastSeenMs<30000);
        uint32_t col=active?c.alertColor:HH_GRAY;
        const char* src[]={"BLE","WiFi","SSID"};
        char detail[64];
        snprintf(detail,sizeof(detail),"%s  %s  %d dBm",
                 c.mac,src[min((int)c.source,2)],c.rssi);
        Display::drawScanRow(y,rh,c.vendor,detail,c.type,c.rssi,false,col);
        y+=rh;
    }
    needRedraw=false;
}

void mode_flockyou() {
    camCount=0;scanning=false;needRedraw=true;
    memset(cams,0,sizeof(cams));
    sdReady=SD.begin();

    BLEDevice::init("HH");
    BLEScan* pScan=BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&flockCB);
    pScan->setActiveScan(false);

    renderFlock2();
    uint32_t lastWiFiScan=0, scanStartMs=0;

    while (true) {
        M5.update();

        if (scanning) {
            // Non-blocking BLE scan
            if (!pScan->isScanning() && millis()-scanStartMs>500) {
                pScan->start(SCAN_SECS,false);
                scanStartMs=millis();
            }
            // WiFi scan every 20 seconds
            if (millis()-lastWiFiScan>20000) {
                lastWiFiScan=millis();
                doWiFiFlock();
            }
        }

        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);

        if (tapped) {
            // Back button area
            if ((tp[0].y>=Display::height()-120 && tp[0].x<240) || tp[0].x<60) break;
            // Start/stop
            if (tp[0].y>=STATUS_H+10 && tp[0].y<=STATUS_H+110 &&
                tp[0].x>=Display::width()-340) {
                scanning=!scanning;
                if (!scanning) pScan->stop();
                else {scanStartMs=0;lastWiFiScan=0;}
                needRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        if (needRedraw) renderFlock2();
        HawkPet::tick();
        delay(20);
    }

    pScan->stop();
    BLEDevice::deinit();
    WiFi.mode(WIFI_OFF);
}
