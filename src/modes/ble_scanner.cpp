// ============================================================
//  HeathenHawk Talon5 — modes/ble_scanner.cpp
//  Full touch UI BLE scanner
//  Passive advertisement sniffing via C6 co-processor
//  Apple Continuity decode, OUI watchlist, device detail view
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#define MODE_NAME   "BLE Scanner"
#define LOG_FILE    "/ble_scan.csv"
#define MAX_DEVICES 300

struct BLEDev {
    char     mac[18];
    char     name[32];
    char     type[24];
    char     vendor[24];
    int8_t   rssi;
    uint32_t lastSeenMs;
    uint32_t seenCount;
    bool     onWatchlist;
};

static BLEDev   devices[MAX_DEVICES];
static uint16_t devCount    = 0;
static int16_t  scrollIdx   = 0;
static bool     sdReady     = false;
static bool     scanning    = false;
static bool     needsRedraw = true;
static int16_t  selectedDev = -1;

// ── Find or add ───────────────────────────────────────────────────────────────
int16_t findOrAdd(const char* mac) {
    for (uint16_t i = 0; i < devCount; i++) {
        if (strcasecmp(devices[i].mac, mac) == 0) return i;
    }
    if (devCount >= MAX_DEVICES) return -1;
    int16_t idx = devCount++;
    memset(&devices[idx], 0, sizeof(BLEDev));
    strlcpy(devices[idx].mac,    mac,          sizeof(devices[idx].mac));
    strlcpy(devices[idx].name,   "?",          sizeof(devices[idx].name));
    strlcpy(devices[idx].type,   "Generic BLE",sizeof(devices[idx].type));
    strlcpy(devices[idx].vendor, "Unknown",    sizeof(devices[idx].vendor));
    return idx;
}

// ── Log to SD ─────────────────────────────────────────────────────────────────
void logBLE(const BLEDev& d) {
    if (!sdReady) return;
    File f = SD.open(LOG_FILE, FILE_APPEND);
    if (!f) return;
    f.printf("%s,\"%s\",%s,%s,%d,%s,%lu\n",
             d.mac, d.name, d.type, d.vendor,
             d.rssi, d.onWatchlist ? "YES" : "no", millis());
    f.close();
}

// ── BLE result callback ───────────────────────────────────────────────────────
void onBLEDev(const BLEResult& r) {
    int16_t idx = findOrAdd(r.mac);
    if (idx < 0) return;

    bool isNew = (devices[idx].seenCount == 0);
    if (strlen(r.name) > 0) strlcpy(devices[idx].name,   r.name,   sizeof(devices[idx].name));
    strlcpy(devices[idx].type,   r.type,   sizeof(devices[idx].type));
    strlcpy(devices[idx].vendor, r.vendor, sizeof(devices[idx].vendor));
    devices[idx].rssi        = r.rssi;
    devices[idx].lastSeenMs  = millis();
    devices[idx].seenCount++;
    devices[idx].onWatchlist = r.onWatchlist;

    if (isNew) {
        logBLE(devices[idx]);
        if (r.onWatchlist) {
            M5.Speaker.tone(1200, 80); delay(100);
            M5.Speaker.tone(1600, 80); delay(100);
            M5.Speaker.tone(2000, 100);
            Display::showAlert("WATCHLIST HIT!", r.vendor, HH_CORAL, 1500);
        }
    }
    needsRedraw = true;
}

// ── Row layout ────────────────────────────────────────────────────────────────
int32_t rowH() { return Display::getOrientation() == ORI_LANDSCAPE ? 54 : 68; }
int32_t visRows() { return (Display::height() - 96) / rowH(); }

// ── Render list ───────────────────────────────────────────────────────────────
void renderBLEList() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, false, 100);

    // Header
    Display::fillRect(0, 48, Display::width(), 40, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 58);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%d BLE devices", devCount);
    Display::print(hdr);

    // Scan button
    uint32_t btnCol = scanning ? HH_CORAL : HH_PURPLE;
    Display::fillRoundRect(Display::width()-140, 54, 124, 28, 8, btnCol);
    Display::setTextColor(HH_WHITE, btnCol);
    Display::setCursor(Display::width()-128, 62);
    Display::print(scanning ? "● Scanning" : "▶ Scan");

    if (devCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.6f);
        Display::setCursor(40, Display::height()/2);
        Display::print("Tap Scan to start");
        return;
    }

    int32_t y = 96;
    for (int16_t i = scrollIdx;
         i < (int16_t)devCount && (i - scrollIdx) < visRows(); i++) {
        BLEDev& d = devices[i];
        bool fresh = (millis() - d.lastSeenMs < 15000);
        uint32_t col = d.onWatchlist ? HH_CORAL :
                       fresh ? HH_PURPLE : HH_GRAY;

        char label[36];
        strlcpy(label, strcmp(d.name,"?") != 0 ? d.name : d.mac, sizeof(label));
        char detail[64];
        snprintf(detail, sizeof(detail), "%s  %s  x%lu",
                 d.type, d.vendor, d.seenCount);

        Display::drawScanRow(y, rowH(), label, detail,
                             d.onWatchlist ? "WATCH" : d.type,
                             d.rssi, (i == selectedDev), col);
        y += rowH();
    }
    needsRedraw = false;
}

// ── Device detail ─────────────────────────────────────────────────────────────
void showBLEDetail(int16_t idx) {
    if (idx < 0 || idx >= (int16_t)devCount) return;
    BLEDev& d = devices[idx];

    Display::clear(HH_DARK);
    Display::drawStatusBar(d.name, false, sdReady, false, 100);
    Display::drawCard(16, 60, Display::width()-32, 340,
                      "BLE Device", d.onWatchlist ? HH_CORAL : HH_PURPLE);

    auto row = [](int32_t y, const char* lbl, const char* val) {
        Display::setTextColor(HH_GRAY, HH_DARKCARD);
        Display::setTextSize(1.3f);
        Display::setCursor(36, y);
        Display::print(lbl);
        Display::setTextColor(HH_WHITE, HH_DARKCARD);
        Display::setCursor(200, y);
        Display::print(val);
    };

    char buf[24];
    row(100, "MAC",    d.mac);
    row(130, "Name",   strcmp(d.name,"?") != 0 ? d.name : "Unknown");
    row(160, "Type",   d.type);
    row(190, "Vendor", d.vendor);
    snprintf(buf, sizeof(buf), "%d dBm", d.rssi);
    row(220, "RSSI",   buf);
    snprintf(buf, sizeof(buf), "%lu", d.seenCount);
    row(250, "Seen",   buf);
    row(280, "Watch",  d.onWatchlist ? "⚠ YES" : "No");

    Display::drawRSSIBar(36, 320, Display::width()-72, 20,
                         d.rssi, d.onWatchlist ? HH_CORAL : HH_PURPLE);

    Display::fillRoundRect(16, Display::height()-80, 160, 52, 12, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setTextSize(1.5f);
    Display::setCursor(48, Display::height()-60);
    Display::print("◀ Back");

    while (true) {
        M5.update();
        if (M5.Touch.getDetail().wasPressed()) break;
        delay(30);
    }
}

// ── Main mode ─────────────────────────────────────────────────────────────────
void mode_ble_scanner() {
    devCount   = 0;
    scrollIdx  = 0;
    scanning   = false;
    needsRedraw = true;
    memset(devices, 0, sizeof(devices));

    sdReady = SD.begin();
    if (sdReady && !SD.exists(LOG_FILE)) {
        File f = SD.open(LOG_FILE, FILE_WRITE);
        if (f) { f.println("MAC,Name,Type,Vendor,RSSI,Watchlist,Timestamp"); f.close(); }
    }

    Comms::onBLEResult(onBLEDev);
    renderBLEList();

    while (true) {
        M5.update();
        Comms::poll();

        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;  // left edge = back

            // Scan button
            if (evt.y >= 54 && evt.y <= 82 && evt.x >= Display::width()-140) {
                scanning = !scanning;
                if (scanning) Comms::startBLEScan();
                else Comms::stopAll();
                needsRedraw = true;
            }
            // List rows
            else if (evt.y > 96) {
                int16_t idx = scrollIdx + (evt.y - 96) / rowH();
                if (idx < (int16_t)devCount) {
                    showBLEDetail(idx);
                    needsRedraw = true;
                }
            }
        }

        // Swipe scroll
        static int32_t lastY = 0;
        if (evt.isPressed()) {
            if (lastY > 0) {
                int32_t dy = lastY - evt.y;
                if (abs(dy) > 20) {
                    scrollIdx = constrain(
                        scrollIdx + (dy > 0 ? 1 : -1),
                        0, max((int16_t)0, (int16_t)(devCount - visRows())));
                    needsRedraw = true;
                    lastY = evt.y;
                }
            } else { lastY = evt.y; }
        } else { lastY = 0; }

        if (needsRedraw) renderBLEList();
        HawkPet::tick();
        delay(16);
    }

    Comms::stopAll();
    Comms::onBLEResult(nullptr);
}
