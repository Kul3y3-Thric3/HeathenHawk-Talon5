// ============================================================
//  HeathenHawk Talon5 — modes/wifi_scanner.cpp
//  WiFi scanner — runs directly on P4 via hosted C6 WiFi driver
//  No co-processor firmware needed — SDIO WiFi works natively
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <SD.h>

#define MODE_NAME    "WiFi Scanner"
#define LOG_FILE     "/wifi_scan.csv"
#define MAX_NETWORKS 200
#define ROW_H        110
#define HEADER_H     120
#define STATUS_H     64

struct WifiNet {
    char     ssid[33];
    char     bssid[18];
    char     auth[16];
    int8_t   rssi;
    int      channel;
    uint32_t lastSeenMs;
    uint32_t seenCount;
};

static WifiNet   networks[MAX_NETWORKS];
static uint16_t  netCount    = 0;
static int16_t   scrollIdx   = 0;
static bool      sdReady     = false;
static bool      scanning    = false;
static bool      needsRedraw = true;
static int16_t   selectedNet = -1;

int16_t findOrAddNet(const char* bssid) {
    for (int i = 0; i < netCount; i++)
        if (strcasecmp(networks[i].bssid, bssid) == 0) return i;
    if (netCount >= MAX_NETWORKS) return -1;
    int16_t idx = netCount++;
    memset(&networks[idx], 0, sizeof(WifiNet));
    strlcpy(networks[idx].bssid, bssid, sizeof(networks[idx].bssid));
    return idx;
}

const char* authStr(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN:         return "OPEN";
        case WIFI_AUTH_WEP:          return "WEP";
        case WIFI_AUTH_WPA_PSK:      return "WPA";
        case WIFI_AUTH_WPA2_PSK:     return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2";
        case WIFI_AUTH_WPA3_PSK:     return "WPA3";
        default:                     return "UNKN";
    }
}

void doScan() {
    // Non-blocking async scan
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false, true);  // async=false, show hidden=true

    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        char bssid[18];
        uint8_t* raw = WiFi.BSSID(i);
        snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                 raw[0],raw[1],raw[2],raw[3],raw[4],raw[5]);

        int16_t idx = findOrAddNet(bssid);
        if (idx < 0) continue;

        bool isNew = (networks[idx].seenCount == 0);
        String ssid = WiFi.SSID(i);
        strlcpy(networks[idx].ssid,  ssid.c_str(), sizeof(networks[idx].ssid));
        strlcpy(networks[idx].auth,
                authStr(WiFi.encryptionType(i)),
                sizeof(networks[idx].auth));
        networks[idx].rssi       = WiFi.RSSI(i);
        networks[idx].channel    = WiFi.channel(i);
        networks[idx].lastSeenMs = millis();
        networks[idx].seenCount++;

        if (isNew && sdReady) {
            File f = SD.open(LOG_FILE, FILE_APPEND);
            if (f) {
                f.printf("%s,\"%s\",%s,%d,%d,%lu\n",
                         networks[idx].bssid, networks[idx].ssid,
                         networks[idx].auth, networks[idx].rssi,
                         networks[idx].channel, millis());
                f.close();
            }
            HawkPet::feed(FEED_WIFI_SCAN, 1);
        }
    }

    WiFi.scanDelete();
    needsRedraw = true;
}

static int32_t visibleRows() {
    return (Display::height() - STATUS_H - HEADER_H) / ROW_H;
}

void drawScanButton() {
    uint32_t col = scanning ? HH_CORAL : HH_GREEN;
    int32_t bw = 320;
    int32_t bh = 80;
    int32_t bx = Display::width() - bw - 20;
    int32_t by = STATUS_H + 20;
    Display::fillRoundRect(bx, by, bw, bh, 16, col);
    Display::setTextColor(HH_WHITE, col);
    Display::setTextSize(3.0f);
    Display::setCursor(bx + 30, by + 20);
    Display::print(scanning ? "  SCANNING..." : "  START SCAN");
}

void renderWifiList() {
    Display::fillRect(0, STATUS_H, Display::width(),
                      Display::height() - STATUS_H, HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdReady, false, 100);

    Display::fillRect(0, STATUS_H, Display::width(), HEADER_H, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(3.5f);
    Display::setCursor(24, STATUS_H + 26);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%d networks found", netCount);
    Display::print(hdr);

    drawScanButton();

    if (netCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setTextSize(3.5f);
        Display::setCursor(40, Display::height()/2);
        Display::print("Tap START SCAN to begin");
        needsRedraw = false;
        return;
    }

    int32_t y = STATUS_H + HEADER_H;
    for (int16_t i = scrollIdx;
         i < netCount && (i - scrollIdx) < visibleRows(); i++) {
        WifiNet& n = networks[i];
        bool fresh = (millis() - n.lastSeenMs < 30000);
        bool sel   = (i == selectedNet);
        uint32_t col = fresh ? HH_TEAL : HH_GRAY;

        char detail[64];
        snprintf(detail, sizeof(detail), "%s  Ch%d  %s  x%lu",
                 n.bssid, n.channel, n.auth, n.seenCount);

        Display::drawScanRow(y, ROW_H,
                             strlen(n.ssid) > 0 ? n.ssid : "[Hidden]",
                             detail, n.auth, n.rssi, sel, col);
        y += ROW_H;
    }
    needsRedraw = false;
}

void showNetDetail(int16_t idx) {
    if (idx < 0 || idx >= netCount) return;
    WifiNet& n = networks[idx];

    Display::clear(HH_DARK);
    Display::drawStatusBar(n.ssid[0] ? n.ssid : "[Hidden]",
                           false, sdReady, false, 100);
    Display::drawCard(20, STATUS_H + 20,
                      Display::width()-40, 480,
                      "Network Details", HH_TEAL);

    auto row = [](int32_t y, const char* lbl, const char* val) {
        Display::setTextColor(HH_GRAY, HH_DARKCARD);
        Display::setTextSize(2.8f);
        Display::setCursor(40, y);
        Display::print(lbl);
        Display::setTextColor(HH_WHITE, HH_DARKCARD);
        Display::setCursor(280, y);
        Display::print(val);
    };

    char buf[32];
    row(STATUS_H + 80,  "SSID",    n.ssid[0] ? n.ssid : "[Hidden]");
    row(STATUS_H + 140, "BSSID",   n.bssid);
    row(STATUS_H + 200, "Auth",    n.auth);
    snprintf(buf, sizeof(buf), "%d", n.channel);
    row(STATUS_H + 260, "Channel", buf);
    snprintf(buf, sizeof(buf), "%d dBm", n.rssi);
    row(STATUS_H + 320, "RSSI",    buf);
    snprintf(buf, sizeof(buf), "%lu times", n.seenCount);
    row(STATUS_H + 380, "Seen",    buf);

    Display::fillRoundRect(20, Display::height()-120,
                           300, 90, 16, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setTextSize(3.2f);
    Display::setCursor(60, Display::height()-92);
    Display::print("BACK");

    while (true) {
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp, 1) > 0) {
            if (tp[0].y > Display::height()-130) {
                while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
                break;
            }
        }
        delay(10);
    }
}

void mode_wifi_scanner() {
    netCount    = 0;
    scrollIdx   = 0;
    scanning    = false;
    needsRedraw = true;
    selectedNet = -1;
    memset(networks, 0, sizeof(networks));

    sdReady = SD.begin();

    renderWifiList();

    uint32_t lastScanMs = 0;

    while (true) {
        M5.update();

        // Auto-scan every 10 seconds when scanning
        if (scanning && millis() - lastScanMs > 10000) {
            lastScanMs = millis();
            Display::setTextColor(HH_AMBER, HH_DARKCARD);
            Display::setTextSize(2.5f);
            Display::setCursor(24, STATUS_H + 80);
            Display::print("Scanning...");
            doScan();
            needsRedraw = true;
        }

        m5::touch_point_t tp[1];
        int num = M5.Lcd.getTouchRaw(tp, 1);
        static bool wasDown = false;
        bool isDown = (num > 0);
        bool tapped = isDown && !wasDown;
        wasDown = isDown;

        if (tapped) {
            int32_t tx = tp[0].x;
            int32_t ty = tp[0].y;

            // Back
            if (tx < 40) break;

            // Scan button
            if (ty >= STATUS_H + 10 && ty <= STATUS_H + 110 &&
                tx >= Display::width() - 360) {
                scanning = !scanning;
                if (scanning) {
                    lastScanMs = 0;  // trigger immediate scan
                }
                needsRedraw = true;
            }
            // Network rows
            else if (ty > STATUS_H + HEADER_H) {
                int16_t idx = scrollIdx +
                              (ty - STATUS_H - HEADER_H) / ROW_H;
                if (idx < netCount) {
                    if (idx == selectedNet) {
                        showNetDetail(idx);
                        selectedNet = -1;
                        needsRedraw = true;
                    } else {
                        selectedNet = idx;
                        needsRedraw = true;
                    }
                }
            }

            while (M5.Lcd.getTouchRaw(tp, 1) > 0) delay(10);
        }

        // Scroll
        static int32_t lastTY = 0;
        if (isDown && tp[0].y > STATUS_H + HEADER_H) {
            if (lastTY > 0) {
                int32_t dy = lastTY - tp[0].y;
                if (abs(dy) > 30) {
                    scrollIdx = constrain(
                        scrollIdx + (dy > 0 ? 1 : -1),
                        0, max(0, netCount - (int)visibleRows()));
                    needsRedraw = true;
                    lastTY = tp[0].y;
                }
            } else lastTY = tp[0].y;
        } else lastTY = 0;

        if (needsRedraw) renderWifiList();
        HawkPet::tick();
        delay(10);
    }

    WiFi.mode(WIFI_OFF);
}
