// ============================================================
//  HeathenHawk Talon5 — comms/comms_manager.h
//  Communication manager for ESP32-C6 and ESP32-C5
//  co-processors
//
//  The P4 sends JSON commands over UART to each radio chip.
//  C6 handles 2.4GHz WiFi + BLE (always present)
//  C5 handles 5GHz WiFi (optional, detected on boot)
// ============================================================

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ── Command types sent to co-processors ──────────────────────────────────────
#define CMD_WIFI_SCAN       "wifi_scan"
#define CMD_WIFI_DEAUTH     "wifi_deauth"
#define CMD_WIFI_BEACON     "wifi_beacon"
#define CMD_WIFI_PORTAL     "wifi_portal"
#define CMD_BLE_SCAN        "ble_scan"
#define CMD_BLE_SPAM        "ble_spam"
#define CMD_WARDRIVING      "wardriving"
#define CMD_SKYSPY          "skyspy"
#define CMD_FLOCKYOU        "flockyou"
#define CMD_STOP            "stop"
#define CMD_EVIL_PORTAL     "evil_portal"
#define CMD_EVIL_PORTAL     "evil_portal"
#define CMD_PING            "ping"
#define CMD_STATUS          "status"

// ── Result types received from co-processors ─────────────────────────────────
#define RESULT_WIFI_NET     "wifi_net"
#define RESULT_BLE_DEV      "ble_dev"
#define RESULT_DRONE        "drone"
#define RESULT_CAMERA       "camera"
#define RESULT_STATUS       "status"
#define RESULT_PONG         "pong"
#define RESULT_ALERT        "alert"

struct WiFiResult {
    char    ssid[33];
    char    bssid[18];
    int8_t  rssi;
    int     channel;
    uint8_t band;       // 0=2.4GHz, 1=5GHz
    char    auth[16];
    char    vendor[24];
};

struct BLEResult {
    char    mac[18];
    char    name[32];
    char    type[24];
    char    vendor[20];
    int8_t  rssi;
    bool    onWatchlist;
};

struct DroneResult {
    char    id[24];
    char    uaType[24];
    float   droneLat;
    float   droneLon;
    float   droneAlt;
    int8_t  rssi;
    uint8_t source;     // 0=BLE, 1=WiFi
};

// ── Callback types ────────────────────────────────────────────────────────────
typedef void (*WiFiResultCb)(const WiFiResult& r);
typedef void (*BLEResultCb)(const BLEResult& r);
typedef void (*DroneResultCb)(const DroneResult& r);
typedef void (*AlertCb)(const char* msg, const char* detail);

namespace Comms {
    // Init
    void begin();

    // Detection
    bool c6Available();
    bool c5Available();
    bool dualBandAvailable();

    // Send commands
    void sendCommand(uint8_t target, const char* cmd, JsonObject params);
    void stopAll();

    // Register callbacks for incoming results
    void onWiFiResult(WiFiResultCb cb);
    void onBLEResult(BLEResultCb cb);
    void onDroneResult(DroneResultCb cb);
    void onAlert(AlertCb cb);

    // Poll for incoming data — call in loop()
    void poll();

    // High level helpers
    void startWiFiScan(bool dual = false);
    void startBLEScan();
    void startWardriving(double lat, double lon);
    void startSkySpy();
    void startFlockYou();
    void startDeauth(const char* bssid, int channel);
    void startBeaconSpam(const char* mode);
    void startBLESpam(const char* target);
    void startEvilPortal(const char* ssid);
}
