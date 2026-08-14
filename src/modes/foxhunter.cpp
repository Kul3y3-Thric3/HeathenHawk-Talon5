// ============================================================
//  HeathenHawk Talon5 — modes/foxhunter.cpp
//  RSSI proximity tracker with target selection
//  Scan first, pick a target, then hunt it
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

#define MODE_NAME "Foxhunter"
#define STATUS_H  64
#define NO_SIG    -127
#define SCAN_SECS 3
#define MAX_TARGETS 30

struct FoxTarget {
    char    id[33];   // SSID or name
    char    mac[18];  // BSSID or BLE MAC
    int8_t  rssi;
    int     channel;  // WiFi only
    bool    isBLE;
};

static FoxTarget targets[MAX_TARGETS];
static uint8_t   targetCount = 0;
static int8_t    huntTarget  = -1;  // index of selected target, -1 = hunt all
static int8_t    targetRSSI  = NO_SIG;
static int8_t    peakRSSI    = NO_SIG;
static uint32_t  lastBeepMs  = 0;
static bool      huntingBLE  = true;

// BLE scan callback — collects all devices
class FoxScanCB : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice adv) {
        if (targetCount >= MAX_TARGETS) return;
        char mac[18];
        strlcpy(mac, adv.getAddress().toString().c_str(), sizeof(mac));
        // Update if already seen
        for (int i=0;i<targetCount;i++) {
            if (strcasecmp(targets[i].mac,mac)==0) {
                targets[i].rssi=adv.getRSSI();
                return;
            }
        }
        strlcpy(targets[targetCount].mac, mac, sizeof(targets[0].mac));
        strlcpy(targets[targetCount].id,
                adv.haveName() ? adv.getName().c_str() : mac,
                sizeof(targets[0].id));
        targets[targetCount].rssi   = adv.getRSSI();
        targets[targetCount].isBLE  = true;
        targetCount++;
    }
};
static FoxScanCB foxScanCB;

// Hunt-mode BLE callback — only updates selected target
static int8_t g_huntRSSI = NO_SIG;
class FoxHuntCB : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice adv) {
        if (huntTarget < 0) {
            // Hunt all — use strongest
            if (adv.getRSSI() > g_huntRSSI || g_huntRSSI==NO_SIG)
                g_huntRSSI = adv.getRSSI();
        } else {
            char mac[18];
            strlcpy(mac, adv.getAddress().toString().c_str(), sizeof(mac));
            if (strcasecmp(mac, targets[huntTarget].mac)==0)
                g_huntRSSI = adv.getRSSI();
        }
    }
};
static FoxHuntCB foxHuntCB;

const char* sigLabel(int8_t r) {
    if (r==NO_SIG) return "NO SIGNAL";
    if (r>-40)     return "VERY CLOSE!";
    if (r>-55)     return "CLOSE";
    if (r>-65)     return "NEARBY";
    if (r>-75)     return "MODERATE";
    if (r>-85)     return "WEAK";
    return                "FAR";
}
uint32_t sigColor(int8_t r) {
    if (r==NO_SIG) return HH_GRAY;
    if (r>-50)     return HH_GREEN;
    if (r>-65)     return HH_TEAL;
    if (r>-75)     return HH_AMBER;
    return                HH_CORAL;
}

void drawRadarMeter(int8_t rssi) {
    int32_t cx=Display::width()/2;
    int32_t cy=Display::height()/2+30;
    int32_t maxR=min(Display::width(),Display::height())/3;
    for (int32_t r=maxR;r>0;r-=maxR/5)
        Display::drawCircle(cx,cy,r,HH_GRAY);
    if (rssi==NO_SIG) return;
    uint8_t pct=constrain(map(rssi,-100,-20,0,100),0,100);
    int32_t fillR=maxR*pct/100;
    uint32_t col=sigColor(rssi);
    for (int32_t r=fillR;r>0;r-=6)
        Display::fillCircle(cx,cy,r,col);
    Display::fillCircle(cx,cy,30,HH_WHITE);
    // dBm in center
    Display::setTextColor(HH_DARK,HH_WHITE);
    Display::setTextSize(2.0f);
    char buf[8]; snprintf(buf,sizeof(buf),"%d",rssi);
    int32_t tw=Display::textWidth(buf);
    Display::setCursor(cx-tw/2,cy-10);
    Display::print(buf);
}

// ── Step 1: BLE or WiFi? ────────────────────────────────────────────────────
bool selectMode() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);
    Display::drawCard(40,100,Display::width()-80,460,"Foxhunter",HH_AMBER);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.2f);
    Display::setCursor(60,160);
    Display::print("Hunt via:");
    Display::fillRoundRect(60,230,Display::width()-120,120,20,HH_PURPLE);
    Display::setTextColor(HH_WHITE,HH_PURPLE);
    Display::setTextSize(3.5f);
    Display::setCursor(80,275);
    Display::print("BLE Device");
    Display::fillRoundRect(60,370,Display::width()-120,120,20,HH_TEAL);
    Display::setTextColor(HH_WHITE,HH_TEAL);
    Display::setCursor(80,415);
    Display::print("WiFi Network");
    Display::fillRoundRect(60,510,Display::width()-120,100,20,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(80,545);
    Display::print("Cancel");

    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            if (tp[0].y>=230&&tp[0].y<=350) { huntingBLE=true;  return true; }
            if (tp[0].y>=370&&tp[0].y<=490) { huntingBLE=false; return true; }
            if (tp[0].y>=510) return false;
        }
        delay(30);
    }
}

// ── Step 2: Scan and pick target ─────────────────────────────────────────────
bool selectTarget() {
    targetCount=0;
    huntTarget=-1;
    memset(targets,0,sizeof(targets));

    // Quick scan to populate target list
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK);
    Display::setTextSize(3.5f);
    Display::setCursor(40,Display::height()/2-40);
    Display::print("Scanning for targets...");
    Display::setTextSize(2.8f);
    Display::setTextColor(HH_GRAY,HH_DARK);
    Display::setCursor(40,Display::height()/2+30);
    Display::print("Please wait 5 seconds");

    if (huntingBLE) {
        BLEDevice::init("HH");
        BLEScan* pScan=BLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(&foxScanCB);
        pScan->setActiveScan(true);
        pScan->start(5, false);  // 5 second blocking scan for discovery
        pScan->stop();
        BLEDevice::getScan()->clearResults();
    } else {
        WiFi.mode(WIFI_STA);
        int n=WiFi.scanNetworks(false,true);
        for (int i=0;i<n&&targetCount<MAX_TARGETS;i++) {
            uint8_t* raw=WiFi.BSSID(i);
            snprintf(targets[targetCount].mac,sizeof(targets[0].mac),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     raw[0],raw[1],raw[2],raw[3],raw[4],raw[5]);
            String ssid=WiFi.SSID(i);
            strlcpy(targets[targetCount].id,
                    ssid.length()>0?ssid.c_str():targets[targetCount].mac,
                    sizeof(targets[0].id));
            targets[targetCount].rssi    = WiFi.RSSI(i);
            targets[targetCount].channel = WiFi.channel(i);
            targets[targetCount].isBLE   = false;
            targetCount++;
        }
        WiFi.scanDelete();
    }

    // Sort by RSSI (strongest first)
    for (int i=0;i<targetCount-1;i++)
        for (int j=0;j<targetCount-i-1;j++)
            if (targets[j].rssi<targets[j+1].rssi) {
                FoxTarget tmp=targets[j]; targets[j]=targets[j+1]; targets[j+1]=tmp;
            }

    if (targetCount==0) {
        Display::showAlert("No Targets Found","Move closer and retry",HH_CORAL,2500);
        return false;
    }

    // Show target list
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);

    Display::fillRect(0,STATUS_H,Display::width(),80,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+22);
    char hdr[32];
    snprintf(hdr,sizeof(hdr),"%d targets found — pick one:",targetCount);
    Display::print(hdr);

    // Hunt All button
    Display::fillRoundRect(20,STATUS_H+90,Display::width()-40,80,14,HH_AMBER);
    Display::setTextColor(HH_WHITE,HH_AMBER);
    Display::setTextSize(3.0f);
    Display::setCursor(40,STATUS_H+112);
    Display::print("Hunt ALL (strongest signal)");

    // Target list
    int32_t y=STATUS_H+182;
    int32_t rh=90;
    int32_t maxRows=(Display::height()-STATUS_H-182)/rh;

    for (int i=0;i<targetCount&&i<maxRows;i++) {
        uint32_t col=sigColor(targets[i].rssi);
        Display::fillRect(0,y,Display::width(),rh-2,HH_DARKCARD);
        Display::drawLine(0,y+rh-2,Display::width(),y+rh-2,HH_DARK);
        Display::fillRect(0,y,6,rh-2,col);
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(20,y+10);
        Display::print(targets[i].id);
        Display::setTextColor(col,HH_DARKCARD);
        Display::setTextSize(2.2f);
        Display::setCursor(20,y+50);
        char det[48];
        snprintf(det,sizeof(det),"%s  %ddBm",targets[i].mac,targets[i].rssi);
        Display::print(det);
        // RSSI bar
        int32_t bw=80;
        int32_t filled=bw*constrain(map(targets[i].rssi,-100,-20,0,100),0,100)/100;
        Display::fillRoundRect(Display::width()-100,y+20,bw,16,6,HH_GRAY);
        if (filled>0) Display::fillRoundRect(Display::width()-100,y+20,filled,16,6,col);
        y+=rh;
    }

    // Wait for selection
    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            if (tp[0].x<60) return false;  // back
            // Hunt all
            if (tp[0].y>=STATUS_H+90&&tp[0].y<=STATUS_H+170) {
                huntTarget=-1;
                return true;
            }
            // Specific target
            if (tp[0].y>=STATUS_H+182) {
                int idx=(tp[0].y-(STATUS_H+182))/rh;
                if (idx>=0&&idx<targetCount) {
                    huntTarget=idx;
                    return true;
                }
            }
        }
        delay(30);
    }
}

// ── Step 3: Hunt screen ──────────────────────────────────────────────────────
void renderHunt() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);

    // Target name
    Display::setTextColor(HH_AMBER,HH_DARK);
    Display::setTextSize(3.5f);
    Display::setCursor(20,STATUS_H+20);
    if (huntTarget>=0) Display::print(targets[huntTarget].id);
    else               Display::print("Hunting ALL devices");

    // Mode
    Display::setTextColor(HH_GRAY,HH_DARK);
    Display::setTextSize(2.5f);
    Display::setCursor(20,STATUS_H+80);
    Display::print(huntingBLE?"BLE":"WiFi");
    if (huntTarget>=0) {
        Display::print("  MAC: ");
        Display::print(huntTarget>=0?targets[huntTarget].mac:"");
    }

    // Signal label
    uint32_t col=sigColor(targetRSSI);
    Display::setTextColor(col,HH_DARK);
    Display::setTextSize(4.5f);
    const char* lbl=sigLabel(targetRSSI);
    int32_t lw=Display::textWidth(lbl);
    Display::setCursor(Display::width()/2-lw/2,STATUS_H+135);
    Display::print(lbl);

    // Radar
    drawRadarMeter(targetRSSI);

    // Peak
    if (peakRSSI!=NO_SIG) {
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(2.5f);
        Display::setCursor(20,Display::height()-110);
        char buf[24]; snprintf(buf,sizeof(buf),"Peak: %d dBm",peakRSSI);
        Display::print(buf);
    }

    // Back button
    Display::fillRoundRect(20,Display::height()-82,200,70,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-62);
    Display::print("BACK");
}

void proximityBeep3(int8_t rssi) {
    if (rssi==NO_SIG) return;
    uint16_t freq=map(constrain(rssi,-100,-20),-100,-20,400,2200);
    uint32_t interval=map(constrain(rssi,-100,-20),-100,-20,2000,80);
    if (millis()-lastBeepMs>interval) {
        lastBeepMs=millis();
        M5.Speaker.tone(freq,40);
    }
}

void mode_foxhunter() {
    targetRSSI=NO_SIG; peakRSSI=NO_SIG; lastBeepMs=0;
    g_huntRSSI=NO_SIG;

    if (!selectMode()) return;
    if (!selectTarget()) {
        if (huntingBLE) BLEDevice::deinit();
        else WiFi.mode(WIFI_OFF);
        return;
    }

    // Switch to hunt mode callbacks
    BLEScan* pScan=nullptr;
    if (huntingBLE) {
        pScan=BLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(&foxHuntCB);
        pScan->setActiveScan(false);
    }

    renderHunt();
    uint32_t scanStartMs=0, lastWiFiScan=0, lastRender=0;
    bool needRedraw=true;

    while (true) {
        M5.update();

        if (huntingBLE) {
            if (!pScan->isScanning()&&millis()-scanStartMs>500) {
                pScan->start(SCAN_SECS,false);
                scanStartMs=millis();
            }
            if (g_huntRSSI!=NO_SIG) {
                targetRSSI=g_huntRSSI;
                if (targetRSSI>peakRSSI||peakRSSI==NO_SIG) peakRSSI=targetRSSI;
                HawkPet::feed(FEED_FOXHUNTER,1);
                g_huntRSSI=NO_SIG;
                needRedraw=true;
            }
        } else {
            if (millis()-lastWiFiScan>3000) {
                lastWiFiScan=millis();
                int n=WiFi.scanNetworks(false,true);
                int8_t best=NO_SIG;
                for (int i=0;i<n;i++) {
                    bool match=(huntTarget<0);
                    if (!match) {
                        uint8_t* raw=WiFi.BSSID(i);
                        char b[18];
                        snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X",
                                 raw[0],raw[1],raw[2],raw[3],raw[4],raw[5]);
                        match=strcasecmp(b,targets[huntTarget].mac)==0;
                    }
                    if (match&&WiFi.RSSI(i)>best) best=WiFi.RSSI(i);
                }
                WiFi.scanDelete();
                if (best!=NO_SIG) {
                    targetRSSI=best;
                    if (best>peakRSSI||peakRSSI==NO_SIG) peakRSSI=best;
                    HawkPet::feed(FEED_FOXHUNTER,1);
                    needRedraw=true;
                }
            }
        }

        proximityBeep3(targetRSSI);

        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);
        if (tapped) {
            if (tp[0].x<60||(tp[0].y>=Display::height()-90&&tp[0].x<240)) break;
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        if (millis()-lastRender>400) { lastRender=millis(); needRedraw=true; }
        if (needRedraw) { renderHunt(); needRedraw=false; }
        HawkPet::tick();
        delay(20);
    }

    if (huntingBLE&&pScan) { pScan->stop(); BLEDevice::deinit(); }
    else WiFi.mode(WIFI_OFF);
    M5.Speaker.stop();
}
