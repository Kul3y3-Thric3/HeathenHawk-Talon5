// ============================================================
//  HeathenHawk Talon5 — modes/rfid.cpp
//  Full 13.56MHz RFID toolkit — Flipper Zero feature parity
//  MFRC522/WS1850S via I2C — Grove Port A G53/G54
//  Supports: MIFARE Classic 1K/4K, MIFARE Ultralight, NTAG2xx
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <MFRC522_I2C.h>
#include <SD.h>

#define MODE_NAME  "RFID"
#define STATUS_H   64
#define LOG_FILE   "/rfid_log.csv"
#define SAVE_DIR   "/rfid_cards"
#define MAX_CARDS  50
#define RFID_ADDR  0x28

MFRC522_I2C rfid(RFID_ADDR, -1, &Wire);

// ── NTAG/UL page write (4 bytes, UL protocol 0xA2) ────────────────────────────
bool writeULPage(byte page, byte* data4) {
    // Build command: 0xA2 + page address + 4 data bytes
    byte sendBuf[6];
    sendBuf[0] = 0xA2;  // UL_WRITE command
    sendBuf[1] = page;
    memcpy(&sendBuf[2], data4, 4);
    byte validBits = 0;
    byte recv[1]; byte recvLen = 1;
    byte status = rfid.PCD_TransceiveData(sendBuf, 6, recv, &recvLen, &validBits, 0, true);
    return (status == MFRC522_I2C::STATUS_OK);
}


// ── Card types ────────────────────────────────────────────────────────────────
enum CardCategory { CAT_UNKNOWN, CAT_MIFARE_CLASSIC, CAT_MIFARE_UL, CAT_NTAG };

struct RFIDCard {
    char         uidStr[22];
    char         type[20];
    uint8_t      uid[10];
    uint8_t      uidSize;
    uint8_t      sak;
    uint16_t     atqa;
    CardCategory category;
    uint32_t     seenMs;
    uint32_t     seenCount;
    // Sector data for MIFARE Classic
    uint8_t      sectorData[64][16];  // 64 blocks x 16 bytes
    bool         sectorRead[16];      // which sectors were read
    uint8_t      foundKey[16][6];     // key found for each sector
    // Page data for NTAG/UL
    uint8_t      pageData[135][4];    // NTAG215 has 135 pages
    uint16_t     pagesRead;
    bool         hasData;
    char         ndefUrl[128];        // extracted NDEF URL if present
    char         ndefText[128];       // extracted NDEF text if present
};

static RFIDCard cards[MAX_CARDS];
static uint8_t  cardCount   = 0;
static bool     sdReady     = false;
static bool     scanning    = false;
static bool     needRedraw  = true;
static int8_t   lastCardIdx = -1;

// ── Key dictionary ────────────────────────────────────────────────────────────
// Expanded Flipper Zero-style key dictionary
static const uint8_t KEYS[][6] = {
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, // Default
    {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5}, // MAD sector A
    {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5}, // Common B
    {0x00,0x00,0x00,0x00,0x00,0x00}, // Zeroes
    {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7}, // NFC Forum
    {0x4D,0x3A,0x99,0xC3,0x51,0xDD}, // Common
    {0x1A,0x98,0x2C,0x7E,0x45,0x9A}, // Common
    {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}, // Common
    {0x71,0x4C,0x5C,0x88,0x6E,0x97}, // Common
    {0x58,0x7E,0xE5,0xF9,0x35,0x0F}, // Common
    {0xA0,0x47,0x8C,0xC3,0x90,0x91}, // Common
    {0x53,0x3C,0xB6,0xC7,0x23,0xF6}, // Common
    {0x8F,0xD0,0xA4,0xF2,0x56,0xE9}, // Common
    {0x0A,0x14,0x0F,0xFF,0x56,0x45}, // Common
    {0xAB,0xCD,0xEF,0x12,0x34,0x56}, // Common sequential
    {0x23,0x45,0x67,0x89,0xAB,0xCD}, // Common sequential 2
};
static const uint8_t KEY_COUNT = sizeof(KEYS)/6;

// ── Helpers ───────────────────────────────────────────────────────────────────
int8_t findOrAddCard(const char* uid) {
    for (uint8_t i=0;i<cardCount;i++)
        if (strcmp(cards[i].uidStr,uid)==0) return i;
    if (cardCount>=MAX_CARDS) return -1;
    int8_t idx=cardCount++;
    memset(&cards[idx],0,sizeof(RFIDCard));
    strlcpy(cards[idx].uidStr,uid,sizeof(cards[idx].uidStr));
    cards[idx].category=CAT_UNKNOWN;
    return idx;
}

CardCategory getCategory(byte piccType, uint8_t uidSize) {
    if (piccType==MFRC522_I2C::PICC_TYPE_MIFARE_1K||
        piccType==MFRC522_I2C::PICC_TYPE_MIFARE_4K) return CAT_MIFARE_CLASSIC;
    if (piccType==MFRC522_I2C::PICC_TYPE_MIFARE_UL) {
        return uidSize==7 ? CAT_NTAG : CAT_MIFARE_UL;
    }
    return CAT_UNKNOWN;
}

// ── Wait for card with timeout ────────────────────────────────────────────────
bool waitForCard(uint32_t timeoutMs=15000) {
    uint32_t start=millis();
    while (millis()-start<timeoutMs) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            return false;
        }
        if (rfid.PICC_IsNewCardPresent()&&rfid.PICC_ReadCardSerial()) return true;
        delay(100);
    }
    return false;
}

// ── Read NTAG pages ───────────────────────────────────────────────────────────
bool readNTAGPages(RFIDCard& card) {
    card.pagesRead=0;
    // Determine page count from UID (NTAG213=45, NTAG215=135, NTAG216=231)
    uint16_t maxPages=135; // default NTAG215
    for (uint16_t page=0;page<maxPages;page+=4) {
        byte buf[18]; byte len=18;
        byte status=rfid.MIFARE_Read(page,buf,&len);
        if (status!=MFRC522_I2C::STATUS_OK) break;
        for (int i=0;i<4&&page+i<maxPages;i++)
            memcpy(card.pageData[page+i],buf+i*4,4);
        card.pagesRead=page+4;
    }
    // Parse NDEF if present (page 4 onward for NTAG)
    if (card.pagesRead>4) {
        // Check for NDEF magic in CC (page 3)
        if (card.pageData[3][0]==0xE1) {
            // Simple URL extraction from page 5+
            uint8_t* ndef=card.pageData[4];
            if (ndef[0]==0x03) { // NDEF message TLV
                uint8_t ndefLen=ndef[1];
                uint8_t* rec=ndef+2;
                if ((rec[0]&0x07)==0x01 && rec[1]==0x01 && rec[3]==0x55) {
                    // URI record
                    const char* prefixes[]={"","http://www.","https://www.",
                                           "http://","https://","tel:",
                                           "mailto:","ftp://","ftps://"};
                    uint8_t prefix=rec[4]<9?rec[4]:0;
                    strlcpy(card.ndefUrl,prefixes[prefix],sizeof(card.ndefUrl));
                    uint8_t urlLen=rec[2]-2;
                    strncat(card.ndefUrl,(char*)(rec+5),
                            min((int)urlLen,(int)(sizeof(card.ndefUrl)-strlen(card.ndefUrl)-1)));
                }
            }
        }
    }
    card.hasData=(card.pagesRead>0);
    return card.hasData;
}

// ── Write NTAG pages ──────────────────────────────────────────────────────────

// ── Simple on-screen keyboard ─────────────────────────────────────────────────
String rfidKeyboard(const char* title, const char* initial="") {
    String input = initial;
    const char* rows[] = {"QWERTYUIOP","ASDFGHJKL ","ZXCVBNM"};
    int rowLens[] = {10, 10, 7};

    auto redraw = [&]() {
        Display::clear(HH_DARK);
        Display::drawStatusBar(title,false,false,false,100);
        // Input box
        Display::fillRoundRect(20,STATUS_H+10,Display::width()-40,80,12,HH_DARKCARD);
        Display::drawRoundRect(20,STATUS_H+10,Display::width()-40,80,12,HH_AMBER);
        Display::setTextColor(input.length()>0?HH_WHITE:HH_GRAY,HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(36,STATUS_H+30);
        Display::print(input.length()>0?input.c_str():"Type here...");

        // Keys
        int32_t kw=64, kh=80, gap=8;
        int32_t startY=STATUS_H+110;
        for (int r=0;r<3;r++) {
            int32_t rowW=rowLens[r]*(kw+gap)-gap;
            int32_t startX=(Display::width()-rowW)/2;
            for (int i=0;i<rowLens[r];i++) {
                int32_t kx=startX+i*(kw+gap);
                int32_t ky=startY+r*(kh+gap);
                bool isSpace=(rows[r][i]==' ');
                Display::fillRoundRect(kx,ky,isSpace?kw*2:kw,kh,8,HH_DARKCARD);
                Display::drawRoundRect(kx,ky,isSpace?kw*2:kw,kh,8,HH_PURPLE);
                if (!isSpace) {
                    Display::setTextColor(HH_WHITE,HH_DARKCARD);
                    Display::setTextSize(2.5f);
                    char ch[2]={rows[r][i],0};
                    Display::setCursor(kx+18,ky+24); Display::print(ch);
                } else {
                    Display::setTextColor(HH_GRAY,HH_DARKCARD);
                    Display::setTextSize(2.0f);
                    Display::setCursor(kx+12,ky+28); Display::print("SPACE");
                }
            }
        }
        // Bottom row: DEL and DONE
        int32_t botY=startY+3*(kh+gap);
        Display::fillRoundRect(20,botY,(Display::width()-60)/2,kh,12,HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_CORAL); Display::setTextSize(3.0f);
        Display::setCursor(40,botY+22); Display::print("DEL");
        Display::fillRoundRect(Display::width()/2+10,botY,(Display::width()-60)/2,kh,12,HH_GREEN);
        Display::setTextColor(HH_WHITE,HH_GREEN);
        Display::setCursor(Display::width()/2+30,botY+22); Display::print("DONE");
    };

    redraw();

    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            int32_t tx=tp[0].x, ty=tp[0].y;
            int32_t kw=64, kh=80, gap=8, startY=STATUS_H+110;

            // Check bottom row
            int32_t botY=startY+3*(kh+gap);
            if (ty>=botY&&ty<=botY+kh) {
                if (tx<Display::width()/2) {
                    if (input.length()>0) input.remove(input.length()-1);
                } else {
                    return input;
                }
                redraw(); continue;
            }

            // Check key rows
            for (int r=0;r<3;r++) {
                int32_t ky=startY+r*(kh+gap);
                if (ty>=ky&&ty<=ky+kh) {
                    int32_t rowW=rowLens[r]*(kw+gap)-gap;
                    int32_t startX=(Display::width()-rowW)/2;
                    for (int i=0;i<rowLens[r];i++) {
                        int32_t kx=startX+i*(kw+gap);
                        bool isSpace=(rows[r][i]==' ');
                        int32_t thisW=isSpace?kw*2:kw;
                        if (tx>=kx&&tx<=kx+thisW) {
                            if (input.length()<50) {
                                input += isSpace?' ':rows[r][i];
                            }
                            redraw(); break;
                        }
                    }
                    break;
                }
            }
        }
        delay(20);
    }
}

bool writeNTAGUrl(const char* url) {
    // Write CC (Capability Container) to page 3 first
    // E1=NDEF magic, 10=version, 3E=size for NTAG215 (504 bytes), 00=access
    byte cc[4]={0xE1,0x10,0x3E,0x00};
    writeULPage(3,cc);

    // Build NDEF URI record
    uint8_t urlLen=min((int)strlen(url),100);
    uint8_t ndefPayload[200]={0};
    int p=0;
    ndefPayload[p++]=0x03; // NDEF TLV tag
    ndefPayload[p++]=urlLen+5; // NDEF message length
    ndefPayload[p++]=0xD1; // MB=1,ME=1,SR=1,TNF=001(Well-known)
    ndefPayload[p++]=0x01; // Type length=1
    ndefPayload[p++]=urlLen+1; // Payload length
    ndefPayload[p++]=0x55; // Type 'U' = URI
    ndefPayload[p++]=0x04; // URI prefix: https://
    memcpy(&ndefPayload[p],url,urlLen); p+=urlLen;
    ndefPayload[p++]=0xFE; // Terminator TLV

    // Write pages starting from page 4
    int totalPages=(p+3)/4;
    for (int pg=0;pg<totalPages;pg++) {
        byte pd[4]={0,0,0,0};
        for (int b=0;b<4&&pg*4+b<p;b++) pd[b]=ndefPayload[pg*4+b];
        if (!writeULPage(4+pg,pd)) return false;
    }
    return true;
}

bool writeNTAGText(const char* text) {
    // Write CC first
    byte cc[4]={0xE1,0x10,0x3E,0x00};
    writeULPage(3,cc);

    uint8_t textLen=min((int)strlen(text),100);
    uint8_t ndefPayload[200]={0};
    int p=0;
    ndefPayload[p++]=0x03;
    ndefPayload[p++]=textLen+7;
    ndefPayload[p++]=0xD1;
    ndefPayload[p++]=0x01;
    ndefPayload[p++]=textLen+3;
    ndefPayload[p++]=0x54; // 'T'
    ndefPayload[p++]=0x02; // UTF-8, lang len=2
    ndefPayload[p++]='e'; ndefPayload[p++]='n';
    memcpy(&ndefPayload[p],text,textLen); p+=textLen;
    ndefPayload[p++]=0xFE;

    int totalPages=(p+3)/4;
    for (int pg=0;pg<totalPages;pg++) {
        byte pd[4]={0,0,0,0};
        for (int b=0;b<4&&pg*4+b<p;b++) pd[b]=ndefPayload[pg*4+b];
        if (!writeULPage(4+pg,pd)) return false;
    }
    return true;
}

// ── Read MIFARE Classic sectors ───────────────────────────────────────────────
int readMifareClassic(RFIDCard& card) {
    int sectorsRead=0;
    int maxSectors=(card.category==CAT_MIFARE_CLASSIC)?16:32;
    MFRC522_I2C::MIFARE_Key key;

    for (int sector=0;sector<maxSectors;sector++) {
        if (card.sectorRead[sector]) { sectorsRead++; continue; }
        int trailerBlock=(sector<32)?(sector*4+3):(sector*16/4+15);
        bool found=false;
        for (int k=0;k<KEY_COUNT&&!found;k++) {
            memcpy(key.keyByte,KEYS[k],6);
            byte status=rfid.PCD_Authenticate(
                MFRC522_I2C::PICC_CMD_MF_AUTH_KEY_A,trailerBlock,&key,&rfid.uid);
            if (status==MFRC522_I2C::STATUS_OK) {
                // Read all blocks in sector
                int startBlock=(sector<32)?sector*4:128+(sector-32)*16;
                int numBlocks=(sector<32)?4:16;
                bool allRead=true;
                for (int b=0;b<numBlocks;b++) {
                    byte buf[18]; byte len=18;
                    if (rfid.MIFARE_Read(startBlock+b,buf,&len)==MFRC522_I2C::STATUS_OK)
                        memcpy(card.sectorData[startBlock+b],buf,16);
                    else allRead=false;
                }
                if (allRead) {
                    memcpy(card.foundKey[sector],KEYS[k],6);
                    card.sectorRead[sector]=true;
                    sectorsRead++;
                }
                rfid.PCD_StopCrypto1();
                found=true;
            }
        }
    }
    card.hasData=(sectorsRead>0);
    return sectorsRead;
}

// ── Write MIFARE Classic block ────────────────────────────────────────────────
bool writeMifareBlock(RFIDCard& card, int block, byte* data) {
    int sector=(block<128)?block/4:(block-128)/16+32;
    int trailerBlock=(sector<32)?(sector*4+3):(sector*16/4+15);
    MFRC522_I2C::MIFARE_Key key;
    memcpy(key.keyByte,card.foundKey[sector],6);
    if (!card.sectorRead[sector]) {
        // Try default key
        memcpy(key.keyByte,KEYS[0],6);
    }
    byte status=rfid.PCD_Authenticate(
        MFRC522_I2C::PICC_CMD_MF_AUTH_KEY_A,trailerBlock,&key,&rfid.uid);
    if (status!=MFRC522_I2C::STATUS_OK) return false;
    status=rfid.MIFARE_Write(block,data,16);
    rfid.PCD_StopCrypto1();
    return status==MFRC522_I2C::STATUS_OK;
}

// ── Save card to SD ───────────────────────────────────────────────────────────
void saveCardToSD(RFIDCard& card) {
    if (!sdReady) { Display::showAlert("No SD","Insert SD card",HH_CORAL,2000); return; }
    if (!SD.exists(SAVE_DIR)) SD.mkdir(SAVE_DIR);
    char fname[22]; strlcpy(fname,card.uidStr,sizeof(fname));
    for (int i=0;fname[i];i++) if (fname[i]==':') fname[i]='-';
    char path[48]; snprintf(path,sizeof(path),"%s/%s.rfid",SAVE_DIR,fname);
    File f=SD.open(path,FILE_WRITE);
    if (!f) return;
    f.printf("UID:%s\n",card.uidStr);
    f.printf("Type:%s\n",card.type);
    f.printf("SAK:0x%02X\n",card.sak);
    f.printf("Size:%d\n",card.uidSize);
    f.printf("Category:%d\n",(int)card.category);
    if (strlen(card.ndefUrl)>0) f.printf("URL:%s\n",card.ndefUrl);
    if (strlen(card.ndefText)>0) f.printf("Text:%s\n",card.ndefText);
    if (card.category==CAT_NTAG||card.category==CAT_MIFARE_UL) {
        for (uint16_t p=0;p<card.pagesRead;p++) {
            f.printf("P%03d:%02X%02X%02X%02X\n",p,
                     card.pageData[p][0],card.pageData[p][1],
                     card.pageData[p][2],card.pageData[p][3]);
        }
    } else {
        for (int b=0;b<64;b++) {
            int sector=b/4;
            if (!card.sectorRead[sector]) continue;
            f.printf("B%03d:",b);
            for (int i=0;i<16;i++) f.printf("%02X",card.sectorData[b][i]);
            f.println();
        }
    }
    f.close();
    Display::showAlert("Saved!",fname,HH_TEAL,1500);
}

// ── Load card from SD ─────────────────────────────────────────────────────────
void loadSavedCards() {
    if (!sdReady||!SD.exists(SAVE_DIR)) {
        Display::showAlert("No Cards","No saved cards on SD",HH_GRAY,2000); return;
    }
    struct SC { char name[32]; char path[48]; };
    static SC sc[20]; uint8_t count=0;
    File dir=SD.open(SAVE_DIR);
    if (!dir) return;
    File entry;
    while ((entry=dir.openNextFile())&&count<20) {
        if (!entry.isDirectory()) {
            strlcpy(sc[count].name,entry.name(),32);
            snprintf(sc[count].path,48,"%s/%s",SAVE_DIR,entry.name());
            count++;
        }
        entry.close();
    }
    dir.close();
    if (count==0){Display::showAlert("Empty","No saved cards",HH_GRAY,2000);return;}

    Display::clear(HH_DARK);
    Display::drawStatusBar("Load Card",false,sdReady,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+20); Display::print("Select card:");
    int32_t y=STATUS_H+80,rh=90;
    for (uint8_t i=0;i<count&&y<Display::height()-110;i++) {
        Display::fillRoundRect(20,y,Display::width()-40,rh-4,12,HH_DARKCARD);
        Display::drawRoundRect(20,y,Display::width()-40,rh-4,12,HH_PURPLE);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(2.8f);
        Display::setCursor(40,y+28); Display::print(sc[i].name);
        y+=rh;
    }
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("BACK");

    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            if (tp[0].y>Display::height()-120) return;
            int32_t ty2=STATUS_H+80;
            for (uint8_t i=0;i<count;i++) {
                if (tp[0].y>=ty2&&tp[0].y<=ty2+86) {
                    File f=SD.open(sc[i].path,FILE_READ);
                    if (!f) return;
                    char uid[22]="",type2[20]="Unknown";
                    uint8_t sak2=0,size2=4; int cat2=0;
                    char url[128]="",text[128]="";
                    RFIDCard loaded; memset(&loaded,0,sizeof(loaded));
                    while (f.available()) {
                        String line=f.readStringUntil('\n');
                        line.trim();
                        if (line.startsWith("UID:")) strlcpy(uid,line.c_str()+4,sizeof(uid));
                        if (line.startsWith("Type:")) strlcpy(type2,line.c_str()+5,sizeof(type2));
                        if (line.startsWith("SAK:")) sak2=(uint8_t)strtol(line.c_str()+6,nullptr,16);
                        if (line.startsWith("Size:")) size2=atoi(line.c_str()+5);
                        if (line.startsWith("Category:")) cat2=atoi(line.c_str()+9);
                        if (line.startsWith("URL:")) strlcpy(url,line.c_str()+4,sizeof(url));
                        if (line.startsWith("Text:")) strlcpy(text,line.c_str()+5,sizeof(text));
                        // Load page data
                        if (line.startsWith("P")&&line.length()>=9) {
                            int pg=atoi(line.c_str()+1);
                            if (pg<135) {
                                const char* hex=line.c_str()+5;
                                for (int b=0;b<4;b++) {
                                    char h[3]={hex[b*2],hex[b*2+1],0};
                                    loaded.pageData[pg][b]=(uint8_t)strtol(h,nullptr,16);
                                }
                                if (pg+1>(int)loaded.pagesRead) loaded.pagesRead=pg+1;
                            }
                        }
                        // Load block data
                        if (line.startsWith("B")&&line.length()>=20) {
                            int blk=atoi(line.c_str()+1);
                            if (blk<64) {
                                const char* hex=line.c_str()+5;
                                for (int b=0;b<16;b++) {
                                    char h[3]={hex[b*2],hex[b*2+1],0};
                                    loaded.sectorData[blk][b]=(uint8_t)strtol(h,nullptr,16);
                                }
                                loaded.sectorRead[blk/4]=true;
                            }
                        }
                    }
                    f.close();
                    if (strlen(uid)>0) {
                        int8_t idx=findOrAddCard(uid);
                        if (idx>=0) {
                            strlcpy(cards[idx].type,type2,sizeof(cards[idx].type));
                            cards[idx].sak=sak2;
                            cards[idx].uidSize=size2;
                            cards[idx].category=(CardCategory)cat2;
                            strlcpy(cards[idx].ndefUrl,url,sizeof(cards[idx].ndefUrl));
                            strlcpy(cards[idx].ndefText,text,sizeof(cards[idx].ndefText));
                            // Copy loaded data
                            memcpy(cards[idx].pageData,loaded.pageData,sizeof(loaded.pageData));
                            cards[idx].pagesRead=loaded.pagesRead;
                            memcpy(cards[idx].sectorData,loaded.sectorData,sizeof(loaded.sectorData));
                            memcpy(cards[idx].sectorRead,loaded.sectorRead,sizeof(loaded.sectorRead));
                            cards[idx].hasData=(cards[idx].pagesRead>0||loaded.sectorRead[0]);
                            cards[idx].seenCount=1;
                            cards[idx].seenMs=millis();
                            lastCardIdx=idx;
                        }
                        Display::showAlert("Loaded!",uid,HH_GREEN,1500);
                    }
                    return;
                }
                ty2+=90;
            }
        }
        delay(30);
    }
}

// ── NDEF Write menu ───────────────────────────────────────────────────────────
void writeNDEFMenu() {
    struct { const char* label; char value[128]; uint32_t color; } opts[] = {
        {"URL: heavensheathens.store",  "heavensheathens.store",    HH_BLUE},
        {"URL: Custom",                 "",                          HH_TEAL},
        {"Text: HeathenHawk",           "HeathenHawk ProTechTor",   HH_PURPLE},
        {"Text: Custom",                "",                          HH_GREEN},
        {"Cancel",                      "",                          HH_GRAY},
    };

    Display::clear(HH_DARK);
    Display::drawStatusBar("Write NDEF",false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+20); Display::print("Select NDEF type:");
    int32_t y=STATUS_H+80,bh=100,gap=12;
    for (int i=0;i<5;i++) {
        Display::fillRoundRect(20,y,Display::width()-40,bh,14,opts[i].color);
        Display::setTextColor(HH_WHITE,opts[i].color); Display::setTextSize(2.8f);
        Display::setCursor(40,y+30); Display::print(opts[i].label);
        y+=bh+gap;
    }

    int chosen=-1;
    while (chosen<0) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            int32_t ty2=STATUS_H+80;
            for (int i=0;i<5;i++) {
                if (tp[0].y>=ty2&&tp[0].y<=ty2+bh) { chosen=i; break; }
                ty2+=bh+gap;
            }
        }
        delay(30);
    }
    if (chosen==4) return;

    // Custom entry via keyboard
    if (chosen==1||chosen==3) {
        String inp = rfidKeyboard(chosen==1?"Enter URL":"Enter Text","");
        if (inp.length()==0) return;
        strlcpy(opts[chosen].value, inp.c_str(), sizeof(opts[chosen].value));
    }

    const char* writeVal = opts[chosen].value;
    if (strlen(writeVal)==0) return;
    bool isURL = (chosen==0||chosen==1);

    Display::clear(HH_DARK);
    Display::drawStatusBar("Write NDEF",false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+60);
    Display::printf("%s:",isURL?"URL":"Text");
    Display::setTextColor(HH_AMBER,HH_DARK); Display::setTextSize(2.8f);
    Display::setCursor(20,STATUS_H+110);
    Display::printf("%.50s",writeVal);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,Display::height()/2-20);
    Display::print("Hold NTAG near unit...");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("CANCEL");

    uint32_t start2=millis();
    while (millis()-start2<20000) {
        M5.update();
        m5::touch_point_t tp2[1];
        if (M5.Lcd.getTouchRaw(tp2,1)>0&&tp2[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp2,1)>0) delay(10); return;
        }
        if (rfid.PICC_IsNewCardPresent()&&rfid.PICC_ReadCardSerial()) {
            bool ok = isURL ? writeNTAGUrl(writeVal) : writeNTAGText(writeVal);
            rfid.PICC_HaltA();
            if (ok) {
                M5.Speaker.tone(1047,100);delay(130);
                M5.Speaker.tone(1319,100);delay(130);
                M5.Speaker.tone(1568,150);
                Display::showAlert("Written!",writeVal,HH_GREEN,2000);
            } else {
                Display::showAlert("Failed","Check tag type\nNTAG215 recommended",HH_CORAL,2500);
            }
            return;
        }
        delay(100);
    }
    Display::showAlert("Timeout","No tag detected",HH_CORAL,1500);
}


// ── Clone/Write card data to blank ────────────────────────────────────────────
void writeCardData(RFIDCard& src) {
    Display::clear(HH_DARK);
    Display::drawStatusBar("Write Card",false,false,false,100);
    Display::drawCard(20,STATUS_H+20,Display::width()-40,480,"Write to Blank Tag",HH_GREEN);
    Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.0f);
    Display::setCursor(40,STATUS_H+80);
    Display::printf("Source: %s",src.uidStr);
    Display::setCursor(40,STATUS_H+140);
    Display::printf("Type: %s",src.type);

    if (!src.hasData) {
        Display::setTextColor(HH_CORAL,HH_DARKCARD);
        Display::setCursor(40,STATUS_H+220);
        Display::print("No data read from source!");
        Display::setCursor(40,STATUS_H+270);
        Display::print("Scan card first to read data.");
        Display::waitForTap();
        return;
    }

    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setCursor(40,STATUS_H+220);
    if (src.category==CAT_NTAG||src.category==CAT_MIFARE_UL)
        Display::printf("%d pages to write",src.pagesRead);
    else
        Display::print("Sector data to write");
    Display::setCursor(40,STATUS_H+290);
    Display::print("Hold blank tag near unit...");
    Display::setCursor(40,STATUS_H+340);
    Display::print("(NTAG215 recommended)");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("CANCEL");

    uint32_t start=millis();
    while (millis()-start<30000) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10); return;
        }
        if (rfid.PICC_IsNewCardPresent()&&rfid.PICC_ReadCardSerial()) {
            byte piccType=rfid.PICC_GetType(rfid.uid.sak);
            int written=0,failed=0;

            if (piccType==MFRC522_I2C::PICC_TYPE_MIFARE_UL) {
                // Write page by page starting from page 4
                for (uint16_t pg=4;pg<src.pagesRead&&pg<135;pg++) {
                    if (rfid.MIFARE_Write(pg,src.pageData[pg],4)==MFRC522_I2C::STATUS_OK)
                        written++;
                    else failed++;
                }
            } else if (piccType==MFRC522_I2C::PICC_TYPE_MIFARE_1K||
                       piccType==MFRC522_I2C::PICC_TYPE_MIFARE_4K) {
                MFRC522_I2C::MIFARE_Key key;
                for (int sector=0;sector<16;sector++) {
                    if (!src.sectorRead[sector]) continue;
                    int trailerBlock=sector*4+3;
                    memcpy(key.keyByte,KEYS[0],6); // try default
                    if (rfid.PCD_Authenticate(MFRC522_I2C::PICC_CMD_MF_AUTH_KEY_A,
                        trailerBlock,&key,&rfid.uid)==MFRC522_I2C::STATUS_OK) {
                        for (int b=0;b<3;b++) { // skip trailer block
                            int blk=sector*4+b;
                            if (blk==0) continue; // never write block 0
                            if (rfid.MIFARE_Write(blk,src.sectorData[blk],16)==MFRC522_I2C::STATUS_OK)
                                written++;
                            else failed++;
                        }
                        rfid.PCD_StopCrypto1();
                    }
                }
            }
            rfid.PICC_HaltA();
            char msg[32]; snprintf(msg,sizeof(msg),"%d written, %d failed",written,failed);
            if (failed==0&&written>0) {
                M5.Speaker.tone(1047,100);delay(130);
                M5.Speaker.tone(1319,100);delay(130);
                M5.Speaker.tone(1568,150);
                Display::showAlert("Write OK!",msg,HH_GREEN,2500);
            } else {
                Display::showAlert(written>0?"Partial":"Failed",msg,HH_CORAL,2500);
            }
            return;
        }
        delay(100);
    }
    Display::showAlert("Timeout","No tag found",HH_CORAL,1500);
}

// ── Render main screen ────────────────────────────────────────────────────────
void renderRFID2() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);
    Display::fillRect(0,STATUS_H,Display::width(),120,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.5f);
    Display::setCursor(24,STATUS_H+26);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d cards",cardCount);
    Display::print(hdr);

    // 4 equal buttons
    int32_t bw=(Display::width()-60)/4, by=STATUS_H+20, bh=80;
    struct { const char* lbl; uint32_t col; } btns[]={
        {scanning?"STOP":"SCAN", scanning?HH_CORAL:HH_GREEN},
        {"WRITE",  HH_AMBER},
        {"SAVE",   HH_TEAL},
        {"LOAD",   HH_PURPLE},
    };
    for (int i=0;i<4;i++) {
        int32_t bx=10+i*(bw+10);
        Display::fillRoundRect(bx,by,bw,bh,12,btns[i].col);
        Display::setTextColor(HH_WHITE,btns[i].col);
        Display::setTextSize(2.2f);
        Display::setCursor(bx+8,by+26);
        Display::print(btns[i].lbl);
    }

    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("BACK");

    if (cardCount==0) {
        Display::setTextColor(HH_GRAY,HH_DARK); Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()/2-60);
        Display::print("Tap SCAN — hold card");
        Display::setCursor(40,Display::height()/2-10);
        Display::print("near RFID unit");
        Display::setTextSize(2.5f);
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setCursor(40,Display::height()/2+50);
        Display::print("Supports: MIFARE Classic");
        Display::setCursor(40,Display::height()/2+90);
        Display::print("MIFARE Ultralight  NTAG2xx");
        needRedraw=false; return;
    }

    if (lastCardIdx>=0) {
        RFIDCard& c=cards[lastCardIdx];
        Display::fillRoundRect(20,STATUS_H+130,Display::width()-40,180,16,HH_DARKCARD);
        Display::drawRoundRect(20,STATUS_H+130,Display::width()-40,180,16,HH_GREEN);
        Display::setTextColor(HH_GREEN,HH_DARKCARD); Display::setTextSize(2.8f);
        Display::setCursor(40,STATUS_H+148);
        Display::printf("LAST SCAN — %s",c.type);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.2f);
        Display::setCursor(40,STATUS_H+190); Display::print(c.uidStr);
        Display::setTextColor(HH_GRAY,HH_DARKCARD); Display::setTextSize(2.5f);
        Display::setCursor(40,STATUS_H+246);
        if (strlen(c.ndefUrl)>0)
            Display::printf("URL: %.50s",c.ndefUrl);
        else if (c.hasData)
            Display::printf("SAK:0x%02X  %s  x%lu scans",
                           c.sak,c.category==CAT_NTAG?"NTAG":"MIFARE",c.seenCount);
        else
            Display::printf("SAK:0x%02X  x%lu scans",c.sak,c.seenCount);
    }

    int32_t y=STATUS_H+330,rh=90;
    for (uint8_t i=0;i<cardCount&&y<Display::height()-130;i++) {
        uint32_t col=(i==(uint8_t)lastCardIdx)?HH_GREEN:HH_GRAY;
        Display::fillRect(0,y,Display::width(),rh-2,HH_DARKCARD);
        Display::drawLine(0,y+rh-2,Display::width(),y+rh-2,HH_DARK);
        Display::fillRect(0,y,6,rh-2,col);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(2.8f);
        Display::setCursor(20,y+10); Display::print(cards[i].uidStr);
        Display::setTextColor(col,HH_DARKCARD); Display::setTextSize(2.2f);
        Display::setCursor(20,y+52);
        Display::printf("%s  x%lu%s",cards[i].type,cards[i].seenCount,
                        cards[i].hasData?" ✓":"");
        y+=rh;
    }
    needRedraw=false;
}

// ── Card detail view ──────────────────────────────────────────────────────────
void showCardDetail(int8_t idx) {
    if (idx<0||idx>=(int8_t)cardCount) return;
    RFIDCard& c=cards[idx];

    Display::clear(HH_DARK);
    Display::drawStatusBar(c.uidStr,false,sdReady,false,100);

    struct { const char* lbl; uint32_t col; } actions[]={
        {"Write Data to Blank Tag", HH_GREEN},
        {"Write NDEF Record",       HH_BLUE},
        {"Save to SD Card",         HH_TEAL},
        {"Read/Crack Keys",         HH_AMBER},
        {"View Raw Data",           HH_PURPLE},
        {"Back",                    HH_GRAY},
    };
    int32_t y=STATUS_H+10,bh=96,gap=10;
    for (int i=0;i<6;i++) {
        Display::fillRoundRect(20,y,Display::width()-40,bh,14,actions[i].col);
        Display::setTextColor(HH_WHITE,actions[i].col); Display::setTextSize(3.0f);
        Display::setCursor(40,y+28); Display::print(actions[i].lbl);
        y+=bh+gap;
    }

    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            int32_t by2=STATUS_H+10;
            for (int i=0;i<6;i++) {
                if (tp[0].y>=by2&&tp[0].y<=by2+bh) {
                    switch(i) {
                        case 0: writeCardData(c); break;
                        case 1: writeNDEFMenu(); break;
                        case 2: saveCardToSD(c); break;
                        case 3:
                            Display::showAlert("Hold Card","Present card to read",HH_AMBER,1500);
                            if (rfid.PICC_IsNewCardPresent()&&rfid.PICC_ReadCardSerial()) {
                                int sec=readMifareClassic(c);
                                if (c.category==CAT_NTAG||c.category==CAT_MIFARE_UL) readNTAGPages(c);
                                rfid.PICC_HaltA();
                                char msg[32]; snprintf(msg,sizeof(msg),"%d sectors read",sec);
                                Display::showAlert(sec>0?"Keys Found!":"Failed",
                                                  sec>0?msg:"No keys worked",
                                                  sec>0?HH_GREEN:HH_CORAL,2000);
                            }
                            break;
                        case 4:
                            // Show raw data
                            Display::clear(HH_DARK);
                            Display::drawStatusBar("Raw Data",false,false,false,100);
                            Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(2.2f);
                            Display::setCursor(10,STATUS_H+20);
                            Display::printf("UID: %s  SAK:0x%02X",c.uidStr,c.sak);
                            if (c.category==CAT_NTAG||c.category==CAT_MIFARE_UL) {
                                int32_t ry=STATUS_H+70;
                                for (uint16_t p=0;p<c.pagesRead&&ry<Display::height()-110;p++) {
                                    Display::setCursor(10,ry);
                                    Display::printf("P%03d: %02X %02X %02X %02X",p,
                                        c.pageData[p][0],c.pageData[p][1],
                                        c.pageData[p][2],c.pageData[p][3]);
                                    ry+=40;
                                }
                            } else {
                                int32_t ry=STATUS_H+70;
                                for (int b=0;b<64&&ry<Display::height()-110;b++) {
                                    if (!c.sectorRead[b/4]) continue;
                                    Display::setCursor(10,ry);
                                    Display::printf("B%02d:",b);
                                    for (int j=0;j<8;j++) Display::printf("%02X",c.sectorData[b][j]);
                                    ry+=40;
                                }
                            }
                            Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
                            Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
                            Display::setCursor(40,Display::height()-90); Display::print("BACK");
                            while (true) {
                                M5.update();
                                m5::touch_point_t tp3[1];
                                if (M5.Lcd.getTouchRaw(tp3,1)>0) {
                                    while (M5.Lcd.getTouchRaw(tp3,1)>0) delay(10); break;
                                }
                                delay(30);
                            }
                            break;
                        case 5: return;
                    }
                    needRedraw=true; return;
                }
                by2+=bh+gap;
            }
        }
        delay(30);
    }
}

// ── Main mode ─────────────────────────────────────────────────────────────────
void mode_rfid() {
    cardCount=0;scanning=false;needRedraw=true;lastCardIdx=-1;
    memset(cards,0,sizeof(cards));
    sdReady=SD.begin();

    Wire.begin(53,54,400000);
    rfid.PCD_Init();
    delay(150);
    byte v=rfid.PCD_ReadRegister(rfid.VersionReg);
    if (v==0x00||v==0xFF) {
        Display::clear(HH_DARK);
        Display::drawStatusBar(MODE_NAME,false,false,false,100);
        Display::drawCard(40,STATUS_H+40,Display::width()-80,260,"RFID2 Not Found",HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.0f);
        Display::setCursor(60,STATUS_H+120); Display::printf("Version: 0x%02X",v);
        Display::setCursor(60,STATUS_H+175); Display::print("Check Grove Port A");
        Display::waitForTap();
        Wire.end(); return;
    }

    if (sdReady&&!SD.exists(LOG_FILE)) {
        File f=SD.open(LOG_FILE,FILE_WRITE);
        if (f){f.println("Timestamp,UID,Type,SAK,Pages,NDEF");f.close();}
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
                    cards[idx].seenCount++; cards[idx].seenMs=millis();
                    cards[idx].uidSize=rfid.uid.size; cards[idx].sak=rfid.uid.sak;
                    memcpy(cards[idx].uid,rfid.uid.uidByte,rfid.uid.size);
                    lastCardIdx=idx;
                    byte t=rfid.PICC_GetType(rfid.uid.sak);
                    String tname=rfid.PICC_GetTypeName(t);
                    strlcpy(cards[idx].type,tname.c_str(),sizeof(cards[idx].type));
                    cards[idx].category=getCategory(t,rfid.uid.size);

                    // Auto-read data
                    if (cards[idx].category==CAT_NTAG||cards[idx].category==CAT_MIFARE_UL)
                        readNTAGPages(cards[idx]);
                    else
                        readMifareClassic(cards[idx]);

                    M5.Speaker.tone(isNew?1200:880,100);
                    if (isNew) {
                        HawkPet::feed(FEED_RFID,1);
                        if (sdReady) {
                            File f=SD.open(LOG_FILE,FILE_APPEND);
                            if (f){
                                f.printf("%lu,%s,%s,0x%02X,%d,%s\n",
                                    millis(),uidStr,cards[idx].type,cards[idx].sak,
                                    cards[idx].pagesRead,cards[idx].ndefUrl);
                                f.close();
                            }
                        }
                    }
                    needRedraw=true;
                }
                rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
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

            if (ty>=STATUS_H+10&&ty<=STATUS_H+110) {
                int32_t bw2=(Display::width()-60)/4;
                int btn=(tx-10)/(bw2+10);
                switch(btn) {
                    case 0: scanning=!scanning; needRedraw=true; break;
                    case 1:
                        scanning=false;
                        if (lastCardIdx>=0) writeNDEFMenu();
                        else Display::showAlert("No Card","Scan first",HH_CORAL,2000);
                        needRedraw=true; break;
                    case 2:
                        if (lastCardIdx>=0) saveCardToSD(cards[lastCardIdx]);
                        else Display::showAlert("No Card","Scan first",HH_CORAL,2000);
                        needRedraw=true; break;
                    case 3:
                        scanning=false; loadSavedCards(); needRedraw=true; break;
                }
            }

            if (ty>=STATUS_H+330) {
                int8_t cidx=(ty-(STATUS_H+330))/90;
                if (cidx>=0&&cidx<(int8_t)cardCount) {
                    scanning=false; showCardDetail(cidx); needRedraw=true;
                }
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
