// ============================================================
//  HeathenHawk Talon5 — modes/skyspy.cpp
//  FAA Remote ID drone detection — BLE + WiFi NAN
//  Full touch UI with drone list and detail view
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#define MODE_NAME "Sky Spy"
#define LOG_FILE  "/skyspy_log.csv"
#define MAX_DRONES 30

struct DroneRec {
    char    id[24];
    char    uaType[24];
    float   droneLat, droneLon, droneAlt;
    int8_t  rssi;
    uint8_t source;
    uint32_t firstSeenMs, lastSeenMs;
    uint32_t frameCount;
};

static DroneRec drones[MAX_DRONES];
static uint8_t  droneCount  = 0;
static bool     sdReady     = false;
static bool     needRedraw  = true;
static bool     scanning    = false;

int8_t findOrAddDrone(const char* id) {
    for (uint8_t i = 0; i < droneCount; i++)
        if (strcasecmp(drones[i].id, id) == 0) return i;
    if (droneCount >= MAX_DRONES) return -1;
    int8_t idx = droneCount++;
    memset(&drones[idx], 0, sizeof(DroneRec));
    strlcpy(drones[idx].id, id, sizeof(drones[idx].id));
    strlcpy(drones[idx].uaType, "Unknown", sizeof(drones[idx].uaType));
    drones[idx].firstSeenMs = millis();
    return idx;
}

void onDroneResult(const DroneResult& r) {
    int8_t idx = findOrAddDrone(r.id);
    if (idx < 0) return;
    bool isNew = (drones[idx].lastSeenMs == 0);
    drones[idx].rssi       = r.rssi;
    drones[idx].droneLat   = r.droneLat;
    drones[idx].droneLon   = r.droneLon;
    drones[idx].droneAlt   = r.droneAlt;
    drones[idx].source     = r.source;
    drones[idx].lastSeenMs = millis();
    drones[idx].frameCount++;
    strlcpy(drones[idx].uaType, r.uaType, sizeof(drones[idx].uaType));

    if (isNew) {
        // Alert tone — ascending triple beep
        M5.Speaker.tone(1200, 100); delay(130);
        M5.Speaker.tone(1600, 100); delay(130);
        M5.Speaker.tone(2000, 150);
        Display::showAlert("DRONE DETECTED!", r.id, HH_BLUE, 1500);
        HawkPet::feed(FEED_SKYSPY, 1);
        needRedraw = true;
    }
}

void renderSkySpy() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, false, 100);

    Display::fillRect(0, 48, Display::width(), 40, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 58);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%d drone%s detected",
             droneCount, droneCount == 1 ? "" : "s");
    Display::print(hdr);

    uint32_t btnCol = scanning ? HH_CORAL : HH_BLUE;
    Display::fillRoundRect(Display::width()-140, 54, 124, 28, 8, btnCol);
    Display::setTextColor(HH_WHITE, btnCol);
    Display::setCursor(Display::width()-128, 62);
    Display::print(scanning ? "● Watching" : "▶ Start");

    if (droneCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.5f);
        Display::setCursor(40, Display::height()/2 - 40);
        Display::print("Listening for FAA Remote ID...");
        Display::setTextSize(1.3f);
        Display::setCursor(40, Display::height()/2 + 10);
        Display::print("ASTM F3411  •  BLE + WiFi NAN");
        Display::setCursor(40, Display::height()/2 + 40);
        Display::print("Passive detection only");
        return;
    }

    int32_t y = 96;
    int32_t rh = 90;
    for (uint8_t i = 0; i < droneCount && y < Display::height()-rh; i++) {
        DroneRec& d = drones[i];
        bool active = (millis() - d.lastSeenMs < 15000);
        uint32_t col = active ? HH_BLUE : HH_GRAY;
        const char* src[] = {"BLE","WiFi"};
        char detail[80];
        snprintf(detail, sizeof(detail), "%s  •  %s  •  Alt:%.0fm  •  x%lu",
                 d.uaType, src[min((int)d.source,1)], d.droneAlt, d.frameCount);
        char coordBuf[32];
        snprintf(coordBuf, sizeof(coordBuf), "%.5f, %.5f",
                 d.droneLat, d.droneLon);
        Display::drawScanRow(y, rh, d.id, detail, src[d.source], d.rssi, false, col);
        // Coordinates sub-line
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.1f);
        Display::setCursor(20, y + 66);
        Display::print(coordBuf);
        y += rh;
    }
    needRedraw = false;
}

void mode_skyspy() {
    droneCount = 0; scanning = false; needRedraw = true;
    memset(drones, 0, sizeof(drones));
    sdReady = SD.begin();
    Comms::onDroneResult(onDroneResult);
    renderSkySpy();

    while (true) {
        M5.update(); Comms::poll();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;
            if (evt.y >= 54 && evt.y <= 82 && evt.x >= Display::width()-140) {
                scanning = !scanning;
                if (scanning) Comms::startSkySpy();
                else Comms::stopAll();
                needRedraw = true;
            }
        }
        if (needRedraw) renderSkySpy();
        HawkPet::tick(); delay(16);
    }
    Comms::stopAll();
    Comms::onDroneResult(nullptr);
}
