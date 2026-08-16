// ============================================================
//  HeathenHawk Talon5 — modes/nfc.cpp
//  Full NFC toolkit — ST25R3916 via M5UnitUnifiedNFC
//  Scan, NDEF read/write, Clone, Save, Load, Emulate
// ============================================================

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
#define LOG_FILE   "/nfc_log.csv"
#define SAVE_DIR   "/nfc_tags"
#define MAX_TAGS   50

using namespace m5::nfc::a;

struct NFCTag {
    char     uidStr[22];
    char     type[20];
    uint8_t  uid[10];
    uint8_t  uidSize;
    uint8_t  sak;
    uint16_t atqa;
    uint32_t seenMs;
    uint32_t seenCount;
    char     ndefUrl[128];
    char     ndefText[128];
    uint8_t  pageData[135][4];
    uint16_t pagesRead;
    bool     hasData;
};

static NFCTag  tags[MAX_TAGS];
static uint8_t tagCount   = 0;
static bool    sdReady    = false;
static bool    scanning   = false;
static bool    needRedraw = true;
static int8_t  lastTagIdx = -1;

static m5::unit::UnitUnified Units;
static m5::unit::UnitNFC     unitNFC;
static m5::nfc::NFCLayerA*   nfc_a_ptr = nullptr;
static bool                  nfcInited = false;

int8_t findOrAddTag(const char* uid) {
    for (uint8_t i=0;i<tagCount;i++)
        if (strcmp(tags[i].uidStr,uid)==0) return i;
    if (tagCount>=MAX_TAGS) return -1;
    int8_t idx=tagCount++;
    memset(&tags[idx],0,sizeof(NFCTag));
    strlcpy(tags[idx].uidStr,uid,sizeof(tags[idx].uidStr));
    return idx;
}

// ── Simple keyboard ────────────────────────────────────────────────────────────
String nfcKeyboard(const char* title, const char* initial="") {
    String input = initial;
    const char* rows[] = {"QWERTYUIOP","ASDFGHJKL ","ZXCVBNM./"};
    int rowLens[] = {10,10,9};

    auto redraw = [&]() {
        Display::clear(HH_DARK);
        Display::drawStatusBar(title,false,false,false,100);
        Display::fillRoundRect(20,STATUS_H+10,Display::width()-40,80,12,HH_DARKCARD);
        Display::drawRoundRect(20,STATUS_H+10,Display::width()-40,80,12,HH_AMBER);
        Display::setTextColor(input.length()>0?HH_WHITE:HH_GRAY,HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(36,STATUS_H+28);
        Display::print(input.length()>0?input.c_str():"Type here...");

        int32_t kw=64,kh=74,gap=6,startY=STATUS_H+110;
        for (int r=0;r<3;r++) {
            int32_t rowW=rowLens[r]*(kw+gap)-gap;
            int32_t startX=(Display::width()-rowW)/2;
            for (int i=0;i<rowLens[r];i++) {
                int32_t kx=startX+i*(kw+gap),ky=startY+r*(kh+gap);
                bool isSpace=(rows[r][i]==' ');
                Display::fillRoundRect(kx,ky,isSpace?kw*2:kw,kh,8,HH_DARKCARD);
                Display::drawRoundRect(kx,ky,isSpace?kw*2:kw,kh,8,HH_TEAL);
                if (!isSpace) {
                    Display::setTextColor(HH_WHITE,HH_DARKCARD);
                    Display::setTextSize(2.5f);
                    char ch[2]={rows[r][i],0};
                    Display::setCursor(kx+18,ky+20); Display::print(ch);
                } else {
                    Display::setTextColor(HH_GRAY,HH_DARKCARD);
                    Display::setTextSize(1.8f);
                    Display::setCursor(kx+12,ky+22); Display::print("SPACE");
                }
            }
        }
        int32_t botY=startY+3*(kh+gap);
        Display::fillRoundRect(20,botY,(Display::width()-60)/2,kh,10,HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_CORAL); Display::setTextSize(2.8f);
        Display::setCursor(40,botY+18); Display::print("DEL");
        Display::fillRoundRect(Display::width()/2+10,botY,(Display::width()-60)/2,kh,10,HH_GREEN);
        Display::setTextColor(HH_WHITE,HH_GREEN);
        Display::setCursor(Display::width()/2+30,botY+18); Display::print("DONE");
    };
    redraw();

    while (true) {
        M5.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            int32_t tx=tp[0].x,ty=tp[0].y;
            int32_t kw=64,kh=74,gap=6,startY=STATUS_H+110;
            int32_t botY=startY+3*(kh+gap);
            if (ty>=botY&&ty<=botY+kh) {
                if (tx<Display::width()/2) { if (input.length()>0) input.remove(input.length()-1); }
                else return input;
                redraw(); continue;
            }
            for (int r=0;r<3;r++) {
                int32_t ky=startY+r*(kh+gap);
                if (ty>=ky&&ty<=ky+kh) {
                    int32_t rowW=rowLens[r]*(kw+gap)-gap;
                    int32_t startX=(Display::width()-rowW)/2;
                    for (int i=0;i<rowLens[r];i++) {
                        int32_t kx=startX+i*(kw+gap);
                        bool isSpace=(rows[r][i]==' ');
                        if (tx>=kx&&tx<=kx+(isSpace?kw*2:kw)) {
                            if (input.length()<60) input+=isSpace?' ':rows[r][i];
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

// ── Save tag to SD ────────────────────────────────────────────────────────────
void saveTagToSD(NFCTag& tag) {
    if (!sdReady){Display::showAlert("No SD","Insert SD card",HH_CORAL,2000);return;}
    if (!SD.exists(SAVE_DIR)) SD.mkdir(SAVE_DIR);
    char fname[22]; strlcpy(fname,tag.uidStr,sizeof(fname));
    for (int i=0;fname[i];i++) if (fname[i]==':') fname[i]='-';
    char path[48]; snprintf(path,sizeof(path),"%s/%s.nfc",SAVE_DIR,fname);
    File f=SD.open(path,FILE_WRITE);
    if (!f) return;
    f.printf("UID:%s\n",tag.uidStr);
    f.printf("Type:%s\n",tag.type);
    f.printf("SAK:0x%02X\n",tag.sak);
    f.printf("ATQA:0x%04X\n",tag.atqa);
    f.printf("Size:%d\n",tag.uidSize);
    if (strlen(tag.ndefUrl)>0) f.printf("URL:%s\n",tag.ndefUrl);
    if (strlen(tag.ndefText)>0) f.printf("Text:%s\n",tag.ndefText);
    for (uint16_t p=0;p<tag.pagesRead;p++)
        f.printf("P%03d:%02X%02X%02X%02X\n",p,
                 tag.pageData[p][0],tag.pageData[p][1],
                 tag.pageData[p][2],tag.pageData[p][3]);
    f.close();
    Display::showAlert("Saved!",fname,HH_TEAL,1500);
}

// ── Load tag from SD ──────────────────────────────────────────────────────────
void loadSavedTags() {
    if (!sdReady||!SD.exists(SAVE_DIR)){
        Display::showAlert("No Tags","No saved tags on SD",HH_GRAY,2000);return;}
    struct SC{char name[32];char path[48];};
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
    if (count==0){Display::showAlert("Empty","No saved tags",HH_GRAY,2000);return;}

    Display::clear(HH_DARK);
    Display::drawStatusBar("Load Tag",false,sdReady,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+20); Display::print("Select tag:");
    int32_t y=STATUS_H+80,rh=90;
    for (uint8_t i=0;i<count&&y<Display::height()-110;i++) {
        Display::fillRoundRect(20,y,Display::width()-40,rh-4,12,HH_DARKCARD);
        Display::drawRoundRect(20,y,Display::width()-40,rh-4,12,HH_TEAL);
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
                    NFCTag loaded; memset(&loaded,0,sizeof(loaded));
                    while (f.available()) {
                        String line=f.readStringUntil('\n'); line.trim();
                        if (line.startsWith("UID:")) strlcpy(loaded.uidStr,line.c_str()+4,sizeof(loaded.uidStr));
                        if (line.startsWith("Type:")) strlcpy(loaded.type,line.c_str()+5,sizeof(loaded.type));
                        if (line.startsWith("SAK:")) loaded.sak=(uint8_t)strtol(line.c_str()+6,nullptr,16);
                        if (line.startsWith("ATQA:")) loaded.atqa=(uint16_t)strtol(line.c_str()+7,nullptr,16);
                        if (line.startsWith("Size:")) loaded.uidSize=atoi(line.c_str()+5);
                        if (line.startsWith("URL:")) strlcpy(loaded.ndefUrl,line.c_str()+4,sizeof(loaded.ndefUrl));
                        if (line.startsWith("Text:")) strlcpy(loaded.ndefText,line.c_str()+5,sizeof(loaded.ndefText));
                        if (line.startsWith("P")&&line.length()>=9) {
                            int pg=atoi(line.c_str()+1);
                            if (pg<135) {
                                const char* h=line.c_str()+5;
                                for (int b=0;b<4;b++) {
                                    char hx[3]={h[b*2],h[b*2+1],0};
                                    loaded.pageData[pg][b]=(uint8_t)strtol(hx,nullptr,16);
                                }
                                if (pg+1>(int)loaded.pagesRead) loaded.pagesRead=pg+1;
                            }
                        }
                    }
                    f.close();
                    if (strlen(loaded.uidStr)>0) {
                        int8_t idx=findOrAddTag(loaded.uidStr);
                        if (idx>=0) {
                            tags[idx]=loaded;
                            tags[idx].seenCount=1;
                            tags[idx].seenMs=millis();
                            tags[idx].hasData=(loaded.pagesRead>0);
                            lastTagIdx=idx;
                        }
                        Display::showAlert("Loaded!",loaded.uidStr,HH_GREEN,1500);
                    }
                    return;
                }
                ty2+=90;
            }
        }
        delay(30);
    }
}

// ── Write NDEF to tag ─────────────────────────────────────────────────────────
void writeNDEFToTag() {
    if (!nfc_a_ptr) return;

    struct { const char* label; char value[128]; uint32_t color; } opts[] = {
        {"URL: heavensheathens.store",  "heavensheathens.store",     HH_BLUE},
        {"URL: Custom",                 "",                           HH_TEAL},
        {"Text: HeathenHawk",           "HeathenHawk by ProTechTor", HH_PURPLE},
        {"Text: Custom",                "",                           HH_GREEN},
        {"Cancel",                      "",                           HH_GRAY},
    };

    Display::clear(HH_DARK);
    Display::drawStatusBar("Write NDEF",false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+20); Display::print("Select NDEF content:");
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

    // Custom keyboard entry
    if (chosen==1||chosen==3) {
        String inp = nfcKeyboard(chosen==1?"Enter URL":"Enter Text","");
        if (inp.length()==0) return;
        strlcpy(opts[chosen].value,inp.c_str(),sizeof(opts[chosen].value));
    }

    const char* writeVal=opts[chosen].value;
    bool isURL=(chosen==0||chosen==1);

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
    Display::print("Hold NTAG near NFC unit...");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("CANCEL");

    uint32_t start=millis();
    while (millis()-start<20000) {
        M5.update(); Units.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10); return;
        }
        PICC tag{};
        if (nfc_a_ptr->detect(tag,200)) {
            m5::nfc::ndef::NDEFLayer ndef{*nfc_a_ptr};
            m5::nfc::ndef::Record rec;
            if (isURL) rec.setURIPayload(writeVal,m5::nfc::ndef::URIProtocol::NA);
            else       rec.setTextPayload(writeVal,"en");
            m5::nfc::ndef::TLV tlv;
            tlv.push_back(rec);
            std::vector<m5::nfc::ndef::TLV> tlvs{tlv};
            bool ok=ndef.write(tag.nfcForumTagType(),tlvs);
            if (ok) {
                M5.Speaker.tone(1047,100);delay(130);
                M5.Speaker.tone(1319,100);delay(130);
                M5.Speaker.tone(1568,150);
                Display::showAlert("Written!",writeVal,HH_GREEN,2000);
            } else {
                Display::showAlert("Failed","Tag not writable\nor wrong type",HH_CORAL,2000);
            }
            return;
        }
        delay(100);
    }
    Display::showAlert("Timeout","No tag found",HH_CORAL,1500);
}

// ── Emulate NFC tag ───────────────────────────────────────────────────────────
void emulateNFCTag(NFCTag& tag) {
    Display::clear(HH_DARK);
    Display::drawStatusBar("NFC Emulate",false,false,false,100);
    Display::drawCard(20,STATUS_H+20,Display::width()-40,500,"Emulating Tag",HH_PURPLE);
    Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.0f);
    Display::setCursor(40,STATUS_H+80); Display::print("Emulating UID:");
    Display::setTextColor(HH_PURPLE,HH_DARKCARD); Display::setTextSize(3.2f);
    Display::setCursor(40,STATUS_H+130); Display::print(tag.uidStr);
    Display::setTextColor(HH_GRAY,HH_DARKCARD); Display::setTextSize(2.5f);
    Display::setCursor(40,STATUS_H+190);
    Display::printf("Type: %s  SAK: 0x%02X",tag.type,tag.sak);
    Display::setTextColor(HH_AMBER,HH_DARKCARD); Display::setTextSize(2.8f);
    Display::setCursor(40,STATUS_H+260);
    Display::print("Hold reader near NFC unit");
    Display::setTextColor(HH_GREEN,HH_DARKCARD); Display::setTextSize(2.8f);
    Display::setCursor(40,STATUS_H+320);
    Display::print("Status: Starting...");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_CORAL);
    Display::setTextColor(HH_WHITE,HH_CORAL); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("STOP");

    // Build PICC from tag data
    PICC picc{};
    picc.size=tag.uidSize;
    memcpy(picc.uid,tag.uid,tag.uidSize);
    picc.sak=tag.sak;
    picc.atqa=tag.atqa;
    if (tag.sak==0x00) picc.type=Type::MIFARE_Ultralight;
    else if (tag.sak==0x08) picc.type=Type::MIFARE_Classic_1K;
    else if (tag.sak==0x18) picc.type=Type::MIFARE_Classic_4K;
    else picc.type=Type::MIFARE_Ultralight;
    picc.blocks=135;

    // Memory buffer — copy saved page data
    static uint8_t emuMem[540];
    memset(emuMem,0,sizeof(emuMem));
    if (tag.hasData) {
        for (uint16_t p=0;p<tag.pagesRead&&p<135;p++)
            memcpy(&emuMem[p*4],tag.pageData[p],4);
    }

    m5::nfc::EmulationLayerA emuLayer(static_cast<m5::unit::UnitST25R3916&>(unitNFC));
    emuLayer.setExpiredTime(120000);

    if (!emuLayer.begin(picc,emuMem,sizeof(emuMem))) {
        Display::showAlert("Error","Emulation start failed",HH_CORAL,2000);
        return;
    }

    uint32_t scanCount=0,lastRender=0;
    auto prevState=m5::nfc::EmulationLayerA::State::Off;

    while (true) {
        M5.update(); Units.update();
        emuLayer.update();
        auto state=emuLayer.state();

        if (state!=prevState) {
            prevState=state;
            if (state==m5::nfc::EmulationLayerA::State::Active) {
                scanCount++;
                M5.Speaker.tone(1200,80);
            }
            // Update status
            Display::fillRect(40,STATUS_H+310,Display::width()-80,50,HH_DARKCARD);
            Display::setTextColor(
                state==m5::nfc::EmulationLayerA::State::Active?HH_GREEN:
                state==m5::nfc::EmulationLayerA::State::Ready?HH_AMBER:HH_GRAY,
                HH_DARKCARD);
            Display::setTextSize(2.5f);
            Display::setCursor(40,STATUS_H+320);
            const char* st="Off";
            if (state==m5::nfc::EmulationLayerA::State::Active) st="ACTIVE — Being Read!";
            else if (state==m5::nfc::EmulationLayerA::State::Ready) st="Ready — Waiting...";
            else if (state==m5::nfc::EmulationLayerA::State::Idle) st="Idle";
            else if (state==m5::nfc::EmulationLayerA::State::Halt) st="Halted";
            Display::print(st);

            if (scanCount>0) {
                Display::fillRect(40,STATUS_H+370,Display::width()-80,40,HH_DARKCARD);
                Display::setTextColor(HH_GREEN,HH_DARKCARD); Display::setTextSize(2.5f);
                Display::setCursor(40,STATUS_H+375);
                Display::printf("Successful reads: %lu",scanCount);
            }
        }

        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10); break;
        }
        delay(10);
    }

    emuLayer.end();
    char msg[32]; snprintf(msg,sizeof(msg),"%lu reads recorded",scanCount);
    Display::showAlert("Done!",msg,HH_PURPLE,2000);
}

// ── Write tag data to blank ───────────────────────────────────────────────────
void writeTagData(NFCTag& src) {
    if (!nfc_a_ptr) return;
    if (!src.hasData||strlen(src.ndefUrl)==0&&strlen(src.ndefText)==0) {
        Display::showAlert("No NDEF Data","Scan tag first to read",HH_CORAL,2000);
        return;
    }
    Display::clear(HH_DARK);
    Display::drawStatusBar("Copy Tag",false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(3.0f);
    Display::setCursor(20,STATUS_H+40); Display::printf("Source: %.30s",src.uidStr);
    if (strlen(src.ndefUrl)>0) {
        Display::setCursor(20,STATUS_H+100); Display::printf("URL: %.50s",src.ndefUrl);
    } else {
        Display::setCursor(20,STATUS_H+100); Display::printf("Text: %.50s",src.ndefText);
    }
    Display::setCursor(20,Display::height()/2);
    Display::print("Hold blank NTAG near unit...");
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("CANCEL");

    uint32_t start=millis();
    while (millis()-start<30000) {
        M5.update(); Units.update();
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0&&tp[0].y>Display::height()-120) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10); return;
        }
        PICC target{};
        if (nfc_a_ptr->detect(target,300)) {
            m5::nfc::ndef::NDEFLayer ndef{*nfc_a_ptr};
            m5::nfc::ndef::Record rec;
            if (strlen(src.ndefUrl)>0)
                rec.setURIPayload(src.ndefUrl,m5::nfc::ndef::URIProtocol::NA);
            else
                rec.setTextPayload(src.ndefText,"en");
            m5::nfc::ndef::TLV tlv;
            tlv.push_back(rec);
            std::vector<m5::nfc::ndef::TLV> tlvs{tlv};
            bool ok=ndef.write(target.nfcForumTagType(),tlvs);
            if (ok) {
                M5.Speaker.tone(1047,100);delay(130);
                M5.Speaker.tone(1319,100);delay(130);
                M5.Speaker.tone(1568,150);
                Display::showAlert("Copied!",src.uidStr,HH_GREEN,2000);
            } else {
                Display::showAlert("Failed","Tag not writable",HH_CORAL,2000);
            }
            return;
        }
        delay(100);
    }
    Display::showAlert("Timeout","No tag found",HH_CORAL,1500);
}

// ── Render ────────────────────────────────────────────────────────────────────
void renderNFC() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);
    Display::fillRect(0,STATUS_H,Display::width(),120,HH_DARKCARD);
    Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.5f);
    Display::setCursor(24,STATUS_H+26);
    char hdr[32]; snprintf(hdr,sizeof(hdr),"%d NFC tags",tagCount);
    Display::print(hdr);

    int32_t bw=(Display::width()-60)/4,by=STATUS_H+20,bh=80;
    struct { const char* lbl; uint32_t col; } btns[]={
        {scanning?"STOP":"SCAN",scanning?HH_CORAL:HH_TEAL},
        {"WRITE",HH_AMBER},{"SAVE",HH_GREEN},{"LOAD",HH_PURPLE},
    };
    for (int i=0;i<4;i++) {
        int32_t bx=10+i*(bw+10);
        Display::fillRoundRect(bx,by,bw,bh,12,btns[i].col);
        Display::setTextColor(HH_WHITE,btns[i].col); Display::setTextSize(2.2f);
        Display::setCursor(bx+8,by+26); Display::print(btns[i].lbl);
    }
    Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90); Display::print("BACK");

    if (tagCount==0) {
        Display::setTextColor(HH_GRAY,HH_DARK); Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()/2-40);
        Display::print("Tap SCAN — hold tag");
        Display::setCursor(40,Display::height()/2+10);
        Display::print("near NFC unit");
        needRedraw=false; return;
    }

    if (lastTagIdx>=0) {
        NFCTag& t=tags[lastTagIdx];
        Display::fillRoundRect(20,STATUS_H+130,Display::width()-40,180,16,HH_DARKCARD);
        Display::drawRoundRect(20,STATUS_H+130,Display::width()-40,180,16,HH_TEAL);
        Display::setTextColor(HH_TEAL,HH_DARKCARD); Display::setTextSize(2.8f);
        Display::setCursor(40,STATUS_H+148);
        Display::printf("LAST — %s",t.type);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.2f);
        Display::setCursor(40,STATUS_H+190); Display::print(t.uidStr);
        Display::setTextColor(HH_GRAY,HH_DARKCARD); Display::setTextSize(2.5f);
        Display::setCursor(40,STATUS_H+246);
        if (strlen(t.ndefUrl)>0) Display::printf("URL: %.50s",t.ndefUrl);
        else if (strlen(t.ndefText)>0) Display::printf("Txt: %.50s",t.ndefText);
        else Display::printf("SAK:0x%02X  x%lu scans",t.sak,t.seenCount);
    }

    int32_t y=STATUS_H+330,rh=90;
    for (uint8_t i=0;i<tagCount&&y<Display::height()-130;i++) {
        uint32_t col=(i==(uint8_t)lastTagIdx)?HH_TEAL:HH_GRAY;
        Display::fillRect(0,y,Display::width(),rh-2,HH_DARKCARD);
        Display::drawLine(0,y+rh-2,Display::width(),y+rh-2,HH_DARK);
        Display::fillRect(0,y,6,rh-2,col);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(2.8f);
        Display::setCursor(20,y+10); Display::print(tags[i].uidStr);
        Display::setTextColor(col,HH_DARKCARD); Display::setTextSize(2.2f);
        Display::setCursor(20,y+52);
        Display::printf("%s  x%lu%s",tags[i].type,tags[i].seenCount,
                        tags[i].hasData?" ✓":"");
        y+=rh;
    }
    needRedraw=false;
}

// ── Tag detail ────────────────────────────────────────────────────────────────
void showTagDetail(int8_t idx) {
    if (idx<0||idx>=(int8_t)tagCount) return;
    NFCTag& t=tags[idx];
    Display::clear(HH_DARK);
    Display::drawStatusBar(t.uidStr,false,sdReady,false,100);

    struct { const char* lbl; uint32_t col; } actions[]={
        {"Write NDEF to Tag",   HH_BLUE},
        {"Copy Data to Tag",    HH_GREEN},
        {"Emulate This Tag",    HH_PURPLE},
        {"Save to SD",          HH_TEAL},
        {"View Raw Pages",      HH_AMBER},
        {"Back",                HH_GRAY},
    };
    int32_t y=STATUS_H+10,bh=94,gap=8;
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
                        case 0: writeNDEFToTag(); break;
                        case 1: writeTagData(t); break;
                        case 2: emulateNFCTag(t); break;
                        case 3: saveTagToSD(t); break;
                        case 4:
                            Display::clear(HH_DARK);
                            Display::drawStatusBar("Raw Pages",false,false,false,100);
                            Display::setTextColor(HH_WHITE,HH_DARK); Display::setTextSize(2.2f);
                            Display::setCursor(10,STATUS_H+10);
                            Display::printf("UID:%s",t.uidStr);
                            {
                                int32_t ry=STATUS_H+60;
                                for (uint16_t p=0;p<t.pagesRead&&ry<Display::height()-110;p++) {
                                    Display::setCursor(10,ry);
                                    Display::printf("P%03d:%02X %02X %02X %02X",p,
                                        t.pageData[p][0],t.pageData[p][1],
                                        t.pageData[p][2],t.pageData[p][3]);
                                    ry+=38;
                                }
                            }
                            Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
                            Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
                            Display::setCursor(40,Display::height()-90); Display::print("BACK");
                            while (true) {
                                M5.update();
                                m5::touch_point_t tp3[1];
                                if (M5.Lcd.getTouchRaw(tp3,1)>0){
                                    while(M5.Lcd.getTouchRaw(tp3,1)>0)delay(10);break;}
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
void mode_nfc() {
    tagCount=0;scanning=false;needRedraw=true;lastTagIdx=-1;
    memset(tags,0,sizeof(tags));
    sdReady=SD.begin();

    Wire.begin(53,54,400000);

    if (!nfcInited) {
        if (Units.add(unitNFC,Wire)&&Units.begin()) {
            nfcInited=true;
            static m5::nfc::NFCLayerA nfc_a_inst{unitNFC};
            nfc_a_ptr=&nfc_a_inst;
        }
    }

    if (!nfcInited||!nfc_a_ptr) {
        Display::clear(HH_DARK);
        Display::drawStatusBar(MODE_NAME,false,false,false,100);
        Display::drawCard(40,STATUS_H+40,Display::width()-80,260,"NFC Init Failed",HH_CORAL);
        Display::setTextColor(HH_WHITE,HH_DARKCARD); Display::setTextSize(3.0f);
        Display::setCursor(60,STATUS_H+120); Display::print("Check Grove Port A");
        Display::setCursor(60,STATUS_H+175); Display::print("NFC unit addr: 0x50");
        Display::fillRoundRect(20,Display::height()-110,200,80,14,HH_GRAY);
        Display::setTextColor(HH_WHITE,HH_GRAY); Display::setTextSize(3.0f);
        Display::setCursor(40,Display::height()-90); Display::print("BACK");
        while (true) {
            M5.update();
            m5::touch_point_t tp[1];
            if (M5.Lcd.getTouchRaw(tp,1)>0){while(M5.Lcd.getTouchRaw(tp,1)>0)delay(10);break;}
            delay(30);
        }
        return;
    }

    if (sdReady&&!SD.exists(LOG_FILE)) {
        File f=SD.open(LOG_FILE,FILE_WRITE);
        if (f){f.println("Timestamp,UID,Type,SAK,URL,Text");f.close();}
    }

    renderNFC();

    while (true) {
        M5.update(); Units.update();

        if (scanning&&nfc_a_ptr) {
            PICC tag{};
            if (nfc_a_ptr->detect(tag,100)) {
                char uidStr[22]="";
                for (uint8_t i=0;i<tag.size;i++) {
                    char hex[4];
                    snprintf(hex,sizeof(hex),i<tag.size-1?"%02X:":"%02X",tag.uid[i]);
                    strlcat(uidStr,hex,sizeof(uidStr));
                }
                int8_t idx=findOrAddTag(uidStr);
                if (idx>=0) {
                    bool isNew=(tags[idx].seenCount==0);
                    tags[idx].seenCount++; tags[idx].seenMs=millis();
                    tags[idx].uidSize=tag.size; tags[idx].sak=tag.sak;
                    tags[idx].atqa=tag.atqa;
                    memcpy(tags[idx].uid,tag.uid,tag.size);
                    lastTagIdx=idx;

                    const char* typeName="Unknown";
                    switch(tag.type) {
                        case Type::MIFARE_Classic_1K: typeName="MIFARE 1K"; break;
                        case Type::MIFARE_Classic_4K: typeName="MIFARE 4K"; break;
                        case Type::MIFARE_Ultralight: typeName="MIFARE UL"; break;
                        case Type::NTAG_213: typeName="NTAG213"; break;
                        case Type::NTAG_215: typeName="NTAG215"; break;
                        case Type::NTAG_216: typeName="NTAG216"; break;
                        default: break;
                    }
                    strlcpy(tags[idx].type,typeName,sizeof(tags[idx].type));

                    // Read NDEF
                    m5::nfc::ndef::NDEFLayer ndef{*nfc_a_ptr};
                    std::vector<m5::nfc::ndef::TLV> tlvs;
                    if (ndef.read(tag.nfcForumTagType(),tlvs)) {
                        for (auto& tlv:tlvs) {
                            for (auto& rec:tlv.records()) {
                                std::string typeStr(rec.type());
                                if (typeStr=="U") {
                                    std::string s=rec.payloadAsString();
                                    strlcpy(tags[idx].ndefUrl,s.c_str(),sizeof(tags[idx].ndefUrl));
                                } else if (typeStr=="T") {
                                    std::string s=rec.payloadAsString();
                                    strlcpy(tags[idx].ndefText,s.c_str(),sizeof(tags[idx].ndefText));
                                }
                            }
                        }
                        tags[idx].hasData=true;
                    }

                    M5.Speaker.tone(isNew?1200:880,100);
                    if (isNew) {
                        HawkPet::feed(FEED_NFC,1);
                        if (sdReady) {
                            File f=SD.open(LOG_FILE,FILE_APPEND);
                            if (f){f.printf("%lu,%s,%s,0x%02X,%s,%s\n",
                                millis(),uidStr,typeName,tag.sak,
                                tags[idx].ndefUrl,tags[idx].ndefText);f.close();}
                        }
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
            int32_t tx=tp[0].x,ty=tp[0].y;
            if ((ty>=Display::height()-120&&tx<240)||tx<60) break;

            if (ty>=STATUS_H+10&&ty<=STATUS_H+110) {
                int32_t bw2=(Display::width()-60)/4;
                int btn=(tx-10)/(bw2+10);
                if (btn<0) btn=0; if (btn>3) btn=3;
                switch(btn) {
                    case 0: scanning=!scanning; needRedraw=true; break;
                    case 1:
                        scanning=false; writeNDEFToTag(); needRedraw=true; break;
                    case 2:
                        if (lastTagIdx>=0) saveTagToSD(tags[lastTagIdx]);
                        else Display::showAlert("No Tag","Scan first",HH_CORAL,2000);
                        needRedraw=true; break;
                    case 3:
                        scanning=false; loadSavedTags(); needRedraw=true; break;
                }
            }

            if (ty>=STATUS_H+330) {
                int8_t tidx=(ty-(STATUS_H+330))/90;
                if (tidx>=0&&tidx<(int8_t)tagCount) {
                    scanning=false; showTagDetail(tidx); needRedraw=true;
                }
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        if (needRedraw) renderNFC();
        HawkPet::tick();
        delay(50);
    }
}
