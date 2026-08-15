#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <M5Utility.h>
#include <Wire.h>
#include <SD.h>

#define MODE_NAME  "NFC"
#define STATUS_H   64
#define LOG_FILE   "/nfc_log.txt"
#define MAX_TAGS   50

using namespace m5::nfc::a;

struct NFCTag {
    char     uidStr[22];
    char     type[16];
    uint32_t seenMs;
    uint32_t seenCount;
};

static NFCTag  tags[MAX_TAGS];
static uint8_t tagCount   = 0;
static bool    sdReady    = false;
static bool    scanning   = false;
static bool    needRedraw = true;
static int8_t  lastTagIdx = -1;

static m5::unit::UnitUnified Units;
static m5::unit::UnitNFC     unitNFC;

int8_t findOrAddTag(const char* uid) {
    for (uint8_t i=0;i<tagCount;i++)
        if (strcmp(tags[i].uidStr,uid)==0) return i;
    if (tagCount>=MAX_TAGS) return -1;
    int8_t idx=tagCount++;
    memset(&tags[idx],0,sizeof(NFCTag));
    strlcpy(tags[idx].uidStr,uid,sizeof(tags[idx].uidStr));
    return idx;
}

void renderNFC2() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);
    Display::fillRect(0,STATUS_H,Display::width(),120,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.5f);
    Display::setCursor(24,STATUS_H+26);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d NFC tags found",tagCount);
    Display::print(hdr);
    uint32_t btnCol=scanning?HH_CORAL:HH_TEAL;
    Display::fillRoundRect(Display::width()-320,STATUS_H+20,300,80,16,btnCol);
    Display::setTextColor(HH_WHITE,btnCol);
    Display::setTextSize(3.0f);
    Display::setCursor(Display::width()-300,STATUS_H+40);
    Display::print(scanning?"  STOP":"  SCAN");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90);
    Display::print("BACK");
    if (tagCount==0) {
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()/2-40);
        Display::print("Tap SCAN then hold");
        Display::setCursor(40,Display::height()/2+10);
        Display::print("NFC tag near unit");
        needRedraw=false; return;
    }
    if (lastTagIdx>=0) {
        NFCTag& t=tags[lastTagIdx];
        Display::fillRoundRect(20,STATUS_H+130,Display::width()-40,160,16,HH_DARKCARD);
        Display::drawRoundRect(20,STATUS_H+130,Display::width()-40,160,16,HH_TEAL);
        Display::setTextColor(HH_TEAL,HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(40,STATUS_H+148);
        Display::print("LAST TAG:");
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(40,STATUS_H+188);
        Display::print(t.uidStr);
        Display::setTextColor(HH_GRAY,HH_DARKCARD);
        Display::setTextSize(2.5f);
        Display::setCursor(40,STATUS_H+238);
        Display::printf("%s  x%lu",t.type,t.seenCount);
    }
    int32_t y=STATUS_H+310,rh=90;
    for (uint8_t i=0;i<tagCount&&y<Display::height()-130;i++) {
        uint32_t col=(i==(uint8_t)lastTagIdx)?HH_TEAL:HH_GRAY;
        Display::fillRect(0,y,Display::width(),rh-2,HH_DARKCARD);
        Display::drawLine(0,y+rh-2,Display::width(),y+rh-2,HH_DARK);
        Display::fillRect(0,y,6,rh-2,col);
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(20,y+10);
        Display::print(tags[i].uidStr);
        Display::setTextColor(col,HH_DARKCARD);
        Display::setTextSize(2.2f);
        Display::setCursor(20,y+52);
        Display::printf("%s  x%lu",tags[i].type,tags[i].seenCount);
        y+=rh;
    }
    needRedraw=false;
}

void mode_nfc() {
    tagCount=0;scanning=false;needRedraw=true;lastTagIdx=-1;
    memset(tags,0,sizeof(tags));
    sdReady=SD.begin();

    Wire.begin(53,54,400000);
    bool ok = Units.add(unitNFC, Wire) && Units.begin();

    if (!ok) {
        Display::clear(HH_DARK);
        Display::drawStatusBar(MODE_NAME,false,false,false,100);
        Display::drawCard(40,STATUS_H+40,Display::width()-80,300,"NFC Init Failed",HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(60,STATUS_H+120);
        Display::print("Check Grove Port A");
        Display::setCursor(60,STATUS_H+180);
        Display::print("NFC unit addr: 0x50");
        Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
        Display::setTextColor(HH_WHITE,HH_GRAY);
        Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()-90);
        Display::print("BACK");
        while (true) {
            M5.update();
            m5::touch_point_t tp[1];
            if (M5.Lcd.getTouchRaw(tp,1)>0) {
                while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
                break;
            }
            delay(30);
        }
        Wire.end();
        return;
    }

    m5::nfc::NFCLayerA nfc_a{unitNFC};
    renderNFC2();

    while (true) {
        M5.update();
        Units.update();

        if (scanning) {
            m5::nfc::a::PICC tag{};
            if (nfc_a.detect(tag)) {
                char uidStr[22]="";
                for (uint8_t i=0;i<tag.size;i++) {
                    char hex[4];
                    snprintf(hex,sizeof(hex),i<tag.size-1?"%02X:":"%02X",
                             tag.uid[i]);
                    strlcat(uidStr,hex,sizeof(uidStr));
                }
                int8_t idx=findOrAddTag(uidStr);
                if (idx>=0) {
                    bool isNew=(tags[idx].seenCount==0);
                    tags[idx].seenCount++;
                    tags[idx].seenMs=millis();
                    lastTagIdx=idx;
                    strlcpy(tags[idx].type,"NFC-A",sizeof(tags[idx].type));
                    M5.Speaker.tone(isNew?1200:880,100);
                    if (isNew&&sdReady) {
                        File f=SD.open(LOG_FILE,FILE_APPEND);
                        if (f){f.printf("[%lu] %s NFC-A\n",millis(),uidStr);f.close();}
                        HawkPet::feed(FEED_NFC,1);
                    }
                    needRedraw=true;
                }
            }
        }

        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);
        if (tapped) {
            if ((tp[0].y>=Display::height()-120&&tp[0].x<240)||tp[0].x<60) break;
            if (tp[0].y>=STATUS_H+10&&tp[0].y<=STATUS_H+110&&
                tp[0].x>=Display::width()-340) {
                scanning=!scanning;
                needRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }
        if (needRedraw) renderNFC2();
        HawkPet::tick();
        delay(50);
    }
    Wire.end();
}
