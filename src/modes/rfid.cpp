#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <MFRC522_I2C.h>
#include <SD.h>

#define MODE_NAME  "RFID Scanner"
#define STATUS_H   64
#define LOG_FILE   "/rfid_log.csv"
#define MAX_CARDS  50
#define RFID_ADDR  0x28

MFRC522_I2C rfid(RFID_ADDR, -1, &Wire);

struct RFIDCard {
    char     uidStr[22];
    char     type[16];
    uint8_t  uid[10];
    uint8_t  uidSize;
    uint32_t seenMs;
    uint32_t seenCount;
};

static RFIDCard cards[MAX_CARDS];
static uint8_t  cardCount   = 0;
static bool     sdReady     = false;
static bool     scanning    = false;
static bool     needRedraw  = true;
static int8_t   lastCardIdx = -1;

int8_t findOrAddCard(const char* uid) {
    for (uint8_t i=0;i<cardCount;i++)
        if (strcmp(cards[i].uidStr,uid)==0) return i;
    if (cardCount>=MAX_CARDS) return -1;
    int8_t idx=cardCount++;
    memset(&cards[idx],0,sizeof(RFIDCard));
    strlcpy(cards[idx].uidStr,uid,sizeof(cards[idx].uidStr));
    return idx;
}

void renderRFID2() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);
    Display::fillRect(0,STATUS_H,Display::width(),120,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.5f);
    Display::setCursor(24,STATUS_H+26);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d cards scanned",cardCount);
    Display::print(hdr);

    // Scan button
    uint32_t btnCol=scanning?HH_CORAL:HH_GREEN;
    Display::fillRoundRect(Display::width()-490,STATUS_H+20,220,80,16,btnCol);
    Display::setTextColor(HH_WHITE,btnCol);
    Display::setTextSize(2.8f);
    Display::setCursor(Display::width()-478,STATUS_H+38);
    Display::print(scanning?"STOP":"SCAN");

    // Clone button
    Display::fillRoundRect(Display::width()-260,STATUS_H+20,240,80,16,HH_AMBER);
    Display::setTextColor(HH_WHITE,HH_AMBER);
    Display::setCursor(Display::width()-248,STATUS_H+38);
    Display::print("CLONE");

    // Back button
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90);
    Display::print("BACK");

    if (cardCount==0) {
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()/2-40);
        Display::print("Tap SCAN then hold");
        Display::setCursor(40,Display::height()/2+10);
        Display::print("card near RFID unit");
        needRedraw=false; return;
    }

    if (lastCardIdx>=0) {
        RFIDCard& c=cards[lastCardIdx];
        Display::fillRoundRect(20,STATUS_H+130,Display::width()-40,160,16,HH_DARKCARD);
        Display::drawRoundRect(20,STATUS_H+130,Display::width()-40,160,16,HH_GREEN);
        Display::setTextColor(HH_GREEN,HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(40,STATUS_H+148);
        Display::print("LAST SCAN:");
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(3.2f);
        Display::setCursor(40,STATUS_H+188);
        Display::print(c.uidStr);
        Display::setTextColor(HH_GRAY,HH_DARKCARD);
        Display::setTextSize(2.5f);
        Display::setCursor(40,STATUS_H+238);
        Display::printf("%s  x%lu scans",c.type,c.seenCount);
    }

    int32_t y=STATUS_H+310,rh=90;
    for (uint8_t i=0;i<cardCount&&y<Display::height()-130;i++) {
        uint32_t col=(i==(uint8_t)lastCardIdx)?HH_GREEN:HH_GRAY;
        Display::fillRect(0,y,Display::width(),rh-2,HH_DARKCARD);
        Display::drawLine(0,y+rh-2,Display::width(),y+rh-2,HH_DARK);
        Display::fillRect(0,y,6,rh-2,col);
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(20,y+10);
        Display::print(cards[i].uidStr);
        Display::setTextColor(col,HH_DARKCARD);
        Display::setTextSize(2.2f);
        Display::setCursor(20,y+52);
        Display::printf("%s  x%lu",cards[i].type,cards[i].seenCount);
        y+=rh;
    }
    needRedraw=false;
}

void rfidCloneCard(const char* srcUID) {
    Display::clear(HH_DARK);
    Display::drawStatusBar("RFID Clone",false,false,false,100);
    Display::drawCard(40,STATUS_H+20,Display::width()-80,380,"Clone Card",HH_GREEN);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.0f);
    Display::setCursor(60,STATUS_H+80);
    Display::print("Source UID:");
    Display::setTextColor(HH_GREEN,HH_DARKCARD);
    Display::setTextSize(3.2f);
    Display::setCursor(60,STATUS_H+130);
    Display::print(srcUID);
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(3.0f);
    Display::setCursor(60,STATUS_H+220);
    Display::print("Hold blank writable card");
    Display::setCursor(60,STATUS_H+270);
    Display::print("near RFID unit...");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90);
    Display::print("CANCEL");

    uint32_t start=millis();
    while (millis()-start<30000) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            return;
        }
        if (rfid.PICC_IsNewCardPresent()&&rfid.PICC_ReadCardSerial()) {
            char targetUID[22]="";
            for (uint8_t i=0;i<rfid.uid.size;i++) {
                char hex[4];
                snprintf(hex,sizeof(hex),i<rfid.uid.size-1?"%02X:":"%02X",rfid.uid.uidByte[i]);
                strlcat(targetUID,hex,sizeof(targetUID));
            }
            byte piccType=rfid.PICC_GetType(rfid.uid.sak);
            bool written=false;
            if (piccType==MFRC522_I2C::PICC_TYPE_MIFARE_UL) {
                byte data[4]={0,0,0,0};
                unsigned int b[4];
                sscanf(srcUID,"%02X:%02X:%02X:%02X",&b[0],&b[1],&b[2],&b[3]);
                for (int i=0;i<4;i++) data[i]=(byte)b[i];
                written=(rfid.MIFARE_Write(4,data,4)==MFRC522_I2C::STATUS_OK);
            } else if (piccType==MFRC522_I2C::PICC_TYPE_MIFARE_1K||
                       piccType==MFRC522_I2C::PICC_TYPE_MIFARE_4K) {
                MFRC522_I2C::MIFARE_Key key;
                memset(key.keyByte,0xFF,6);
                byte status=rfid.PCD_Authenticate(
                    MFRC522_I2C::PICC_CMD_MF_AUTH_KEY_A,1,&key,&(rfid.uid));
                if (status==MFRC522_I2C::STATUS_OK) {
                    byte data[16]={0};
                    unsigned int b[4];
                    sscanf(srcUID,"%02X:%02X:%02X:%02X",&b[0],&b[1],&b[2],&b[3]);
                    for (int i=0;i<4;i++) data[i]=(byte)b[i];
                    written=(rfid.MIFARE_Write(1,data,16)==MFRC522_I2C::STATUS_OK);
                }
            }
            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();
            if (written) {
                M5.Speaker.tone(1047,100);delay(130);
                M5.Speaker.tone(1319,100);delay(130);
                M5.Speaker.tone(1568,150);
                if (sdReady) {
                    File f=SD.open("/rfid_clones.txt",FILE_APPEND);
                    if (f){f.printf("Source:%s->Target:%s\n",srcUID,targetUID);f.close();}
                }
                Display::showAlert("Cloned!",srcUID,HH_GREEN,2000);
            } else {
                Display::showAlert("Failed","Not writable or\nauth failed",HH_CORAL,2000);
            }
            return;
        }
        delay(100);
    }
    Display::showAlert("Timeout","No card presented",HH_CORAL,1500);
}

void mode_rfid() {
    cardCount=0;scanning=false;needRedraw=true;lastCardIdx=-1;
    memset(cards,0,sizeof(cards));
    sdReady=SD.begin();
    Wire.begin(53,54,400000);
    rfid.PCD_Init();
    delay(150);
    byte v=rfid.PCD_ReadRegister(rfid.VersionReg);
    bool ok=(v!=0x00&&v!=0xFF);
    if (!ok) {
        Display::clear(HH_DARK);
        Display::drawStatusBar(MODE_NAME,false,false,false,100);
        Display::drawCard(40,STATUS_H+40,Display::width()-80,320,"RFID2 Not Found",HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(60,STATUS_H+120);
        Display::printf("Version: 0x%02X",v);
        Display::setCursor(60,STATUS_H+180);
        Display::print("Check Grove Port A");
        Display::setCursor(60,STATUS_H+240);
        Display::print("RFID2 unit I2C 0x28");
        Display::waitForTap();
        Wire.end();
        return;
    }

    // Init SD log
    if (sdReady&&!SD.exists(LOG_FILE)) {
        File f=SD.open(LOG_FILE,FILE_WRITE);
        if (f){f.println("UID,Type,Timestamp");f.close();}
    }

    renderRFID2();

    while (true) {
        M5.update();
        if (scanning) {
            if (rfid.PICC_IsNewCardPresent()&&rfid.PICC_ReadCardSerial()) {
                char uidStr[22]="";
                for (uint8_t i=0;i<rfid.uid.size;i++) {
                    char hex[4];
                    snprintf(hex,sizeof(hex),i<rfid.uid.size-1?"%02X:":"%02X",rfid.uid.uidByte[i]);
                    strlcat(uidStr,hex,sizeof(uidStr));
                }
                int8_t idx=findOrAddCard(uidStr);
                if (idx>=0) {
                    bool isNew=(cards[idx].seenCount==0);
                    cards[idx].seenCount++;
                    cards[idx].seenMs=millis();
                    cards[idx].uidSize=rfid.uid.size;
                    memcpy(cards[idx].uid,rfid.uid.uidByte,rfid.uid.size);
                    lastCardIdx=idx;
                    byte t=rfid.PICC_GetType(rfid.uid.sak);
                    String tname=rfid.PICC_GetTypeName(t);
                    strlcpy(cards[idx].type,tname.c_str(),sizeof(cards[idx].type));
                    M5.Speaker.tone(isNew?1200:880,100);
                    if (isNew) {
                        HawkPet::feed(FEED_RFID,1);
                        if (sdReady) {
                            File f=SD.open(LOG_FILE,FILE_APPEND);
                            if (f){f.printf("%s,%s,%lu\n",uidStr,cards[idx].type,millis());f.close();}
                        }
                    }
                    needRedraw=true;
                }
                rfid.PICC_HaltA();
                rfid.PCD_StopCrypto1();
            }
        }
        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);
        if (tapped) {
            int32_t tx=tp[0].x,ty=tp[0].y;
            if ((ty>=Display::height()-120&&tx<240)||tx<60) break;
            // Scan button
            if (ty>=STATUS_H+10&&ty<=STATUS_H+110&&
                tx>=Display::width()-500&&tx<Display::width()-260) {
                scanning=!scanning;
                needRedraw=true;
            }
            // Clone button
            if (ty>=STATUS_H+10&&ty<=STATUS_H+110&&
                tx>=Display::width()-265) {
                scanning=false;
                if (lastCardIdx>=0) {
                    rfidCloneCard(cards[lastCardIdx].uidStr);
                } else {
                    Display::showAlert("No Card","Scan a card first",HH_CORAL,2000);
                }
                needRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }
        if (needRedraw) renderRFID2();
        HawkPet::tick();
        delay(50);
    }
    rfid.PCD_Reset();
    Wire.end();
}
