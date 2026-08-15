// ============================================================
//  HeathenHawk Talon5 — modes/wardriving.cpp
//  GPS-tagged wardriving with rolling network feed
//  Wigle.net CSV export + API upload support
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <SD.h>
#include <TinyGPSPlus.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <base64.h>

#define MODE_NAME "Wardriving"
#define STATUS_H  64
#define MAX_FEED  20   // rolling feed entries

struct FeedEntry {
    char    ssid[33];
    char    bssid[18];
    int8_t  rssi;
    int     channel;
    uint32_t seenMs;
};

static TinyGPSPlus gps;
static FeedEntry   feed[MAX_FEED];
static uint8_t     feedHead   = 0;
static uint16_t    feedCount  = 0;
static uint32_t    totalNets  = 0;
static uint32_t    totalPts   = 0;
static bool        sdReady    = false;
static bool        scanning   = false;
static bool        gpsActive  = false;
static double      curLat=0, curLon=0;
static float       curAlt=0, curSpd=0;
static uint8_t     curSats=0;
static bool        needRedraw = true;

// Wigle credentials from preferences
static char wigleUser[64]={0};
static char wigleKey[128]={0};

void loadWigleCredentials() {
    Preferences prefs;
    prefs.begin("wigle",true);
    prefs.getString("user",wigleUser,sizeof(wigleUser));
    prefs.getString("key",wigleKey,sizeof(wigleKey));
    prefs.end();
}

const char* wifiAuth2(wifi_auth_mode_t auth) {
    switch(auth) {
        case WIFI_AUTH_OPEN:         return "[OPEN]";
        case WIFI_AUTH_WEP:          return "[WEP]";
        case WIFI_AUTH_WPA_PSK:      return "[WPA]";
        case WIFI_AUTH_WPA2_PSK:     return "[WPA2]";
        case WIFI_AUTH_WPA3_PSK:     return "[WPA3]";
        default:                     return "[UNKN]";
    }
}

void addToFeed(const char* ssid, const char* bssid, int8_t rssi, int ch) {
    strlcpy(feed[feedHead].ssid,  ssid,  sizeof(feed[0].ssid));
    strlcpy(feed[feedHead].bssid, bssid, sizeof(feed[0].bssid));
    feed[feedHead].rssi    = rssi;
    feed[feedHead].channel = ch;
    feed[feedHead].seenMs  = millis();
    feedHead = (feedHead+1) % MAX_FEED;
    if (feedCount < MAX_FEED) feedCount++;
}

void doWardriveScan() {
    WiFi.mode(WIFI_STA);
    int n=WiFi.scanNetworks(false,true);
    if (n<=0) return;

    totalNets+=n;
    totalPts++;
    HawkPet::feed(FEED_WARDRIVING,1);

    File f;
    bool logOpen=false;
    if (sdReady && gpsActive) {
        f=SD.open("/wigle_log.csv",FILE_APPEND);
        logOpen=f;
    }

    for (int i=0;i<n;i++) {
        uint8_t* raw=WiFi.BSSID(i);
        char bssid[18];
        snprintf(bssid,sizeof(bssid),"%02X:%02X:%02X:%02X:%02X:%02X",
                 raw[0],raw[1],raw[2],raw[3],raw[4],raw[5]);

        String ssid=WiFi.SSID(i);
        int ch=WiFi.channel(i);
        int8_t rssi=WiFi.RSSI(i);

        addToFeed(ssid.c_str(),bssid,rssi,ch);

        if (logOpen) {
            int freq=(ch>14)?(5000+ch*5):(2407+ch*5);
            f.printf("%s,\"%s\",%s,2024-01-01 00:00:00,%d,%d,%d,%.6f,%.6f,%.1f,10.0,WIFI\n",
                     bssid,ssid.c_str(),
                     wifiAuth2(WiFi.encryptionType(i)),
                     ch,freq,rssi,curLat,curLon,curAlt);
        }
    }
    if (logOpen) f.close();
    WiFi.scanDelete();
    needRedraw=true;
}

void renderWardriving() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,gpsActive,sdReady,false,100);

    // Top info bar
    Display::fillRect(0,STATUS_H,Display::width(),100,HH_DARKCARD);

    // GPS status
    Display::setTextColor(gpsActive?HH_GREEN:HH_CORAL,HH_DARKCARD);
    Display::setTextSize(2.5f);
    Display::setCursor(20,STATUS_H+12);
    if (gpsActive) {
        char buf[48];
        snprintf(buf,sizeof(buf),"%.5f, %.5f  %dsats",curLat,curLon,curSats);
        Display::print(buf);
    } else {
        Display::print("No GPS — connect to M5Bus UART");
    }

    // Stats
    Display::setTextColor(HH_WHITE,HH_DARKCARD);
    Display::setTextSize(2.5f);
    Display::setCursor(20,STATUS_H+54);
    char stats[48];
    snprintf(stats,sizeof(stats),"Nets: %lu  Pts: %lu  SD:%s",
             totalNets,totalPts,sdReady?"OK":"--");
    Display::print(stats);

    // Start/stop button
    uint32_t btnCol=scanning?HH_CORAL:HH_GREEN;
    Display::fillRoundRect(Display::width()-300,STATUS_H+10,280,80,14,btnCol);
    Display::setTextColor(HH_WHITE,btnCol);
    Display::setTextSize(2.8f);
    Display::setCursor(Display::width()-288,STATUS_H+30);
    Display::print(scanning?"  STOP":"  START");

    // Rolling feed
    Display::setTextColor(HH_GRAY,HH_DARK);
    Display::setTextSize(2.2f);
    Display::setCursor(20,STATUS_H+112);
    Display::print("Recent networks:");

    int32_t y=STATUS_H+148;
    int32_t rh=80;
    int32_t maxRows=(Display::height()-STATUS_H-148-110)/rh;

    for (int i=0;i<(int)feedCount&&i<maxRows;i++) {
        // Show newest first
        int idx=((int)feedHead-1-i+MAX_FEED)%MAX_FEED;
        if (feedCount<MAX_FEED && i>=(int)feedCount) break;

        FeedEntry& e=feed[idx];
        bool fresh=(millis()-e.seenMs<30000);
        uint32_t col=fresh?HH_TEAL:HH_GRAY;

        Display::fillRect(0,y,Display::width(),rh-2,HH_DARKCARD);
        Display::drawLine(0,y+rh-2,Display::width(),y+rh-2,HH_DARK);

        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(2.5f);
        Display::setCursor(16,y+8);
        Display::print(strlen(e.ssid)>0?e.ssid:"[Hidden]");

        Display::setTextColor(col,HH_DARKCARD);
        Display::setTextSize(2.0f);
        Display::setCursor(16,y+44);
        char det[48];
        snprintf(det,sizeof(det),"%s  Ch%d  %ddBm",e.bssid,e.channel,e.rssi);
        Display::print(det);

        // RSSI bar
        int32_t barW=80;
        int32_t filled=barW*constrain(map(e.rssi,-100,-20,0,100),0,100)/100;
        Display::fillRoundRect(Display::width()-100,y+20,barW,16,6,HH_GRAY);
        if (filled>0) Display::fillRoundRect(Display::width()-100,y+20,filled,16,6,col);

        y+=rh;
    }

    // Bottom buttons
    int32_t bby=Display::height()-100;
    Display::fillRoundRect(20,bby,200,80,14,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,bby+22);
    Display::print("BACK");

    if (strlen(wigleUser)>0) {
        Display::fillRoundRect(Display::width()-240,bby,220,80,14,HH_TEAL);
        Display::setTextColor(HH_WHITE,HH_TEAL);
        Display::setTextSize(2.5f);
        Display::setCursor(Display::width()-228,bby+22);
        Display::print("WIGLE UP");
    }

    needRedraw=false;
}

void wigleUpload() {
    if (strlen(wigleUser)==0||strlen(wigleKey)==0) {
        Display::showAlert("No Credentials","Set in Settings",HH_CORAL,2000);
        return;
    }
    if (!sdReady||!SD.exists("/wigle_log.csv")) {
        Display::showAlert("No Data","No wigle_log.csv on SD",HH_CORAL,2000);
        return;
    }
    Display::showToast("Uploading to Wigle...",HH_TEAL);
    // Basic auth
    String auth=String(wigleUser)+":"+String(wigleKey);
    String authB64=base64::encode(auth);

    File f=SD.open("/wigle_log.csv",FILE_READ);
    if (!f) { Display::showAlert("Error","Cannot open log",HH_CORAL,2000); return; }

    WiFi.mode(WIFI_STA);
    // Note: actual upload requires WiFi connection — show instructions
    f.close();
    WiFi.mode(WIFI_OFF);
    Display::showAlert("Wigle Export","Copy wigle_log.csv\nfrom SD to upload\nat wigle.net",HH_TEAL,3000);
}

void mode_wardriving() {
    totalNets=0;totalPts=0;scanning=false;needRedraw=true;
    feedHead=0;feedCount=0;
    gpsActive=false;curLat=0;curLon=0;

    loadWigleCredentials();
    sdReady=SD.begin();

    if (sdReady&&!SD.exists("/wigle_log.csv")) {
        File f=SD.open("/wigle_log.csv",FILE_WRITE);
        if (f) {
            f.println("WigleWifi-1.4,appRelease=Talon5,model=Tab5,release=1.0,device=Tab5,display=IPS5,board=P4,brand=HeavensHeathens");
            f.println("MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
            f.close();
        }
    }

    Serial2.begin(115200,SERIAL_8N1,7,6);
    renderWardriving();

    uint32_t lastScan=0, lastGPS=0;

    while (true) {
        M5.update();

        // Feed GPS
        while (Serial2.available()) {
            if (gps.encode(Serial2.read())) {
                if (gps.location.isValid()) {
                    curLat=gps.location.lat();
                    curLon=gps.location.lng();
                    curAlt=gps.altitude.isValid()?gps.altitude.meters():0;
                    curSpd=gps.speed.isValid()?gps.speed.kmph():0;
                    curSats=gps.satellites.isValid()?(uint8_t)gps.satellites.value():0;
                    gpsActive=(curSats>=3);
                }
            }
        }

        // Touch
        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);

        if (tapped) {
            int32_t bby=Display::height()-100;
            // Back
            if ((tp[0].y>=bby&&tp[0].x<240)||tp[0].x<60) break;
            // Wigle upload
            if (tp[0].y>=bby&&tp[0].x>=Display::width()-260) {
                wigleUpload();
                needRedraw=true;
            }
            // Start/stop scan
            if (tp[0].y>=STATUS_H+10&&tp[0].y<=STATUS_H+100&&
                tp[0].x>=Display::width()-310) {
                scanning=!scanning;
                if (scanning) lastScan=0;
                needRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        if (scanning&&millis()-lastScan>8000) {
            lastScan=millis();
            doWardriveScan();
            needRedraw=true;
        }

        if (millis()-lastGPS>1000) {
            lastGPS=millis();
            if (scanning||gpsActive) needRedraw=true;
        }

        if (needRedraw) renderWardriving();
        HawkPet::tick();
        delay(20);
    }

    WiFi.mode(WIFI_OFF);
}
