// ============================================================
//  HeathenHawk Talon5 — modes/wardriving.cpp
//  GPS-tagged WiFi wardriving with live stats on 5" display
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <TinyGPSPlus.h>

#define MODE_NAME "Wardriving"
#define WIGLE_FILE "/wigle_log.csv"
#define WIGLE_HDR  "WigleWifi-1.4,appRelease=Talon5,model=Tab5,release=1.0,device=Tab5,display=IPS5,board=P4,brand=HeavensHeathens\nMAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type"

static TinyGPSPlus gps;
static uint32_t totalNets  = 0;
static uint32_t totalPts   = 0;
static bool     sdReady    = false;
static bool     scanning   = false;
static bool     gpsActive  = false;
static double   curLat     = 0, curLon = 0;
static float    curAlt     = 0, curSpd = 0;
static uint8_t  curSats    = 0;
static bool     needRedraw = true;

void onWardrivingNet(const WiFiResult& r) {
    totalNets++;
    HawkPet::feed(FEED_WARDRIVING, 1);

    if (!sdReady || !gpsActive) return;
    File f = SD.open(WIGLE_FILE, FILE_APPEND);
    if (!f) return;
    int freq = (r.channel > 14) ? (5000 + r.channel*5) : (2407 + r.channel*5);
    f.printf("%s,\"%s\",[%s],2024-01-01 00:00:00,%d,%d,%d,%.6f,%.6f,%.1f,%.1f,%s\n",
             r.bssid, r.ssid, r.auth, r.channel, freq, r.rssi,
             curLat, curLon, curAlt, 10.0f,
             r.band == 0 ? "WIFI" : "WIFI5");
    f.close();
    needRedraw = true;
}

void renderWardriving() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, gpsActive, sdReady, false, 100);

    // GPS panel
    Display::drawGPSPanel(curLat, curLon, curSpd, curSats, gpsActive);

    // Stats cards
    int32_t cw = (Display::width() - 48) / 2;
    Display::drawCard(16, 220, cw, 110, "Networks", HH_GREEN);
    Display::setTextColor(HH_GREEN, HH_DARKCARD);
    Display::setTextSize(4.0f);
    Display::setCursor(36, 255);
    char buf[12]; snprintf(buf, sizeof(buf), "%lu", totalNets);
    Display::print(buf);

    Display::drawCard(32 + cw, 220, cw, 110, "Scan Points", HH_TEAL);
    Display::setTextColor(HH_TEAL, HH_DARKCARD);
    Display::setTextSize(4.0f);
    Display::setCursor(52 + cw, 255);
    snprintf(buf, sizeof(buf), "%lu", totalPts);
    Display::print(buf);

    // Status
    Display::setTextColor(sdReady ? HH_GREEN : HH_CORAL, HH_DARK);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 350);
    Display::print(sdReady ? "Logging to wigle_log.csv" : "No SD card");

    // Scan button
    uint32_t btnCol = scanning ? HH_CORAL : HH_GREEN;
    int32_t bw = 300;
    Display::fillRoundRect((Display::width()-bw)/2, Display::height()-90,
                           bw, 60, 16, btnCol);
    Display::setTextColor(HH_WHITE, btnCol);
    Display::setTextSize(1.8f);
    Display::setCursor((Display::width()-bw)/2 + 20, Display::height()-68);
    Display::print(scanning ? "■  Stop Wardriving" : "▶  Start Wardriving");

    needRedraw = false;
}

void mode_wardriving() {
    totalNets = 0; totalPts = 0; scanning = false; needRedraw = true;
    gpsActive = false; curLat = 0; curLon = 0;

    sdReady = SD.begin();
    if (sdReady && !SD.exists(WIGLE_FILE)) {
        File f = SD.open(WIGLE_FILE, FILE_WRITE);
        if (f) { f.println(WIGLE_HDR); f.close(); }
    }

    // GPS on Serial2
    Serial2.begin(9600, SERIAL_8N1, M5BUS_UART_RX, M5BUS_UART_TX);
    Comms::onWiFiResult(onWardrivingNet);
    renderWardriving();

    uint32_t lastScanMs = 0;
    uint32_t lastGPSRender = 0;

    while (true) {
        M5.update(); Comms::poll();

        // Feed GPS
        while (Serial2.available()) {
            if (gps.encode(Serial2.read())) {
                if (gps.location.isValid()) {
                    curLat = gps.location.lat();
                    curLon = gps.location.lng();
                    curAlt = gps.altitude.isValid() ? gps.altitude.meters() : 0;
                    curSpd = gps.speed.isValid() ? gps.speed.kmph() : 0;
                    curSats = gps.satellites.isValid() ? gps.satellites.value() : 0;
                    gpsActive = (curSats >= 3);
                }
            }
        }

        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;
            // Start/stop button
            if (evt.y > Display::height()-100) {
                scanning = !scanning;
                if (scanning) {
                    Comms::startWiFiScan(true);
                    lastScanMs = millis();
                } else {
                    Comms::stopAll();
                }
                needRedraw = true;
            }
        }

        // Auto rescan every 6s
        if (scanning && millis() - lastScanMs > 6000) {
            lastScanMs = millis();
            totalPts++;
            Comms::startWiFiScan(true);
            needRedraw = true;
        }

        // Refresh GPS display every second
        if (millis() - lastGPSRender > 1000) {
            lastGPSRender = millis();
            needRedraw = true;
        }

        if (needRedraw) renderWardriving();
        HawkPet::tick(); delay(16);
    }

    Comms::stopAll();
    Comms::onWiFiResult(nullptr);
}
