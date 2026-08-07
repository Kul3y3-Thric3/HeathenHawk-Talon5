// ============================================================
//  HeathenHawk Talon5 — modes/wifi_scanner.cpp
//  WiFi scanner with full touch UI
//  Dual-band 2.4GHz + 5GHz via C6 + C5 co-processors
// ============================================================

#include "../pins.h"
#include "../display/display_driver.h"
#include "../comms/comms_manager.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include <string>
#include <SD.h>

#define MODE_NAME    "WiFi Scanner"
#define LOG_FILE     "/wifi_scan.csv"
#define MAX_NETWORKS 200

struct WifiNet {
    char     ssid[33];
    char     bssid[18];
    char     auth[16];
    char     vendor[24];
    int8_t   rssi;
    int      channel;
    uint8_t  band;       // 0=2.4GHz, 1=5GHz
    uint32_t lastSeenMs;
    uint32_t seenCount;
};

static WifiNet   networks[MAX_NETWORKS];
static uint16_t  netCount    = 0;
static int16_t   scrollIdx   = 0;
static bool      sdReady     = false;
static bool      scanning    = false;
static uint32_t  lastScanMs  = 0;
static bool      needsRedraw = true;
static int16_t   selectedNet = -1;

// ── Sort by RSSI ──────────────────────────────────────────────────────────────
void sortNetworks() {
    for (int i = 0; i < netCount-1; i++) {
        for (int j = 0; j < netCount-i-1; j++) {
            if (networks[j].rssi < networks[j+1].rssi) {
                WifiNet tmp = networks[j];
                networks[j] = networks[j+1];
                networks[j+1] = tmp;
            }
        }
    }
}

// ── Find or add network ───────────────────────────────────────────────────────
int16_t findOrAddNet(const char* bssid) {
    for (int i = 0; i < netCount; i++) {
        if (strcasecmp(networks[i].bssid, bssid) == 0) return i;
    }
    if (netCount >= MAX_NETWORKS) return -1;
    int16_t idx = netCount++;
    memset(&networks[idx], 0, sizeof(WifiNet));
    strlcpy(networks[idx].bssid, bssid, sizeof(networks[idx].bssid));
    return idx;
}

// ── SD logging ────────────────────────────────────────────────────────────────
void logWifiNet(const WifiNet& n) {
    if (!sdReady) return;
    File f = SD.open(LOG_FILE, FILE_APPEND);
    if (!f) return;
    f.printf("%s,\"%s\",%s,%s,%d,%d,%s,%lu\n",
             n.bssid, n.ssid, n.auth, n.vendor,
             n.rssi, n.channel,
             n.band == 0 ? "2.4GHz" : "5GHz",
             millis());
    f.close();
}

// ── WiFi result callback ──────────────────────────────────────────────────────
void onWiFiNet(const WiFiResult& r) {
    int16_t idx = findOrAddNet(r.bssid);
    if (idx < 0) return;

    bool isNew = (networks[idx].seenCount == 0);
    strlcpy(networks[idx].ssid,   r.ssid,   sizeof(networks[idx].ssid));
    strlcpy(networks[idx].auth,   r.auth,   sizeof(networks[idx].auth));
    strlcpy(networks[idx].vendor, r.vendor, sizeof(networks[idx].vendor));
    networks[idx].rssi       = r.rssi;
    networks[idx].channel    = r.channel;
    networks[idx].band       = r.band;
    networks[idx].lastSeenMs = millis();
    networks[idx].seenCount++;

    if (isNew) logWifiNet(networks[idx]);
    needsRedraw = true;
}

// ── Row height based on orientation ──────────────────────────────────────────
int32_t rowHeight() {
    return Display::getOrientation() == ORI_LANDSCAPE ? 56 : 70;
}

int32_t visibleRows() {
    return (Display::height() - 100) / rowHeight();
}

// ── Render network list ───────────────────────────────────────────────────────
void renderWifiList() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, Comms::c5Available(), 100);

    // Header bar
    Display::fillRect(0, 48, Display::width(), 40, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 58);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "%d networks  2.4GHz%s",
             netCount, Comms::c5Available() ? " + 5GHz" : "");
    Display::print(hdr);

    // Scan button
    uint32_t btnCol = scanning ? HH_CORAL : HH_GREEN;
    Display::fillRoundRect(Display::width()-140, 54, 124, 28, 8, btnCol);
    Display::setTextColor(HH_WHITE, btnCol);
    Display::setCursor(Display::width()-128, 62);
    Display::print(scanning ? "● Scanning" : "▶ Scan");

    // Network rows
    if (netCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(1.6f);
        Display::setCursor(40, Display::height()/2 - 20);
        Display::print("Tap Scan to start");
        return;
    }

    int32_t y = 96;
    int32_t rh = rowHeight();

    for (int16_t i = scrollIdx;
         i < netCount && (i - scrollIdx) < visibleRows(); i++) {

        WifiNet& n = networks[i];
        bool sel = (i == selectedNet);
        bool fresh = (millis() - n.lastSeenMs < 10000);

        // Band color
        uint32_t bandCol = n.band == 1 ? HH_TEAL : HH_PURPLE;
        if (!fresh) bandCol = HH_GRAY;

        // Label
        char label[36];
        strlcpy(label, strlen(n.ssid) > 0 ? n.ssid : "[Hidden]", sizeof(label));

        // Detail
        char detail[64];
        snprintf(detail, sizeof(detail), "%s  Ch%d  %s  %s",
                 n.bssid, n.channel,
                 n.band == 0 ? "2.4G" : "5GHz",
                 n.vendor);

        // Badge
        char badge[8];
        strlcpy(badge, n.auth, sizeof(badge));

        Display::drawScanRow(y, rh, label, detail, badge,
                             n.rssi, sel, bandCol);

        // Touch target for selection
        y += rh;
    }

    needsRedraw = false;
}

// ── Network detail view ───────────────────────────────────────────────────────
void showNetDetail(int16_t idx) {
    if (idx < 0 || idx >= netCount) return;
    WifiNet& n = networks[idx];

    Display::clear(HH_DARK);
    Display::drawStatusBar(n.ssid[0] ? n.ssid : "[Hidden]",
                           false, sdReady, false, 100);

    Display::drawCard(16, 60, Display::width()-32, 320, "Network Details",
                      n.band == 1 ? HH_TEAL : HH_PURPLE);

    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.4f);

    auto row = [](int32_t y, const char* label, const char* val) {
        Display::setTextColor(HH_GRAY, HH_DARKCARD);
        Display::setCursor(36, y);
        Display::print(label);
        Display::setTextColor(HH_WHITE, HH_DARKCARD);
        Display::setCursor(200, y);
        Display::print(val);
    };

    char buf[32];
    row(100,  "SSID",    n.ssid[0] ? n.ssid : "[Hidden]");
    row(130,  "BSSID",   n.bssid);
    row(160,  "Vendor",  n.vendor[0] ? n.vendor : "Unknown");
    row(190,  "Auth",    n.auth);
    snprintf(buf, sizeof(buf), "%d", n.channel);
    row(220,  "Channel", buf);
    row(250,  "Band",    n.band == 0 ? "2.4GHz" : "5GHz");
    snprintf(buf, sizeof(buf), "%d dBm", n.rssi);
    row(280,  "RSSI",    buf);
    snprintf(buf, sizeof(buf), "%lu", n.seenCount);
    row(310,  "Seen",    buf);

    // RSSI bar
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.3f);
    Display::setCursor(36, 360);
    Display::print("Signal strength");
    Display::drawRSSIBar(36, 385, Display::width()-72, 20,
                         n.rssi, n.band == 1 ? HH_TEAL : HH_PURPLE);

    // Back button
    Display::fillRoundRect(16, Display::height()-80, 160, 52, 12, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setTextSize(1.5f);
    Display::setCursor(48, Display::height()-60);
    Display::print("◀ Back");

    // Wait for tap
    while (true) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            // Back button area
            if (evt.y > Display::height()-90) break;
        }
        delay(30);
    }
}

// ── Main mode ─────────────────────────────────────────────────────────────────
void mode_wifi_scanner() {
    netCount    = 0;
    scrollIdx   = 0;
    selectedNet = -1;
    needsRedraw = true;
    scanning    = false;
    memset(networks, 0, sizeof(networks));

    sdReady = SD.begin();
    if (sdReady && !SD.exists(LOG_FILE)) {
        File f = SD.open(LOG_FILE, FILE_WRITE);
        if (f) { f.println("BSSID,SSID,Auth,Vendor,RSSI,Channel,Band,Timestamp"); f.close(); }
    }

    Comms::onWiFiResult(onWiFiNet);

    renderWifiList();

    while (true) {
        M5.update();
        Comms::poll();

        // Back gesture — swipe from left edge or long press
        auto evt = M5.Touch.getDetail();

        if (evt.wasPressed()) {
            int32_t tx = evt.x;
            int32_t ty = evt.y;

            // Scan button
            if (ty >= 54 && ty <= 82 && tx >= Display::width()-140) {
                if (!scanning) {
                    scanning = true;
                    lastScanMs = millis();
                    Comms::startWiFiScan(Comms::c5Available());
                    needsRedraw = true;
                } else {
                    Comms::stopAll();
                    scanning = false;
                    needsRedraw = true;
                }

            // Network rows
            } else if (ty > 96) {
                int32_t rh  = rowHeight();
                int16_t idx = scrollIdx + (ty - 96) / rh;
                if (idx < netCount) {
                    selectedNet = idx;
                    showNetDetail(idx);
                    selectedNet = -1;
                    needsRedraw = true;
                }
            }

            // Left edge swipe = back
            if (tx < 30) break;
        }

        // Scroll via swipe
        static int32_t lastTouchY = 0;
        if (evt.isPressed()) {
            if (lastTouchY > 0) {
                int32_t dy = lastTouchY - evt.y;
                if (abs(dy) > 20) {
                    scrollIdx = constrain(scrollIdx + (dy > 0 ? 1 : -1),
                                         0, max((int16_t)0, (int16_t)(netCount - visibleRows())));
                    needsRedraw = true;
                    lastTouchY = evt.y;
                }
            } else {
                lastTouchY = evt.y;
            }
        } else {
            lastTouchY = 0;
        }

        // Auto-rescan every 8 seconds
        if (scanning && millis() - lastScanMs > 8000) {
            lastScanMs = millis();
            sortNetworks();
            Comms::startWiFiScan(Comms::c5Available());
            needsRedraw = true;
        }

        if (needsRedraw) renderWifiList();

        HawkPet::tick();
        delay(16);
    }

    Comms::stopAll();
    Comms::onWiFiResult(nullptr);
}
