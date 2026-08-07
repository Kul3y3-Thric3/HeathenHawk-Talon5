// ============================================================
//  HeathenHawk Talon5 — comms/comms_manager.cpp
//  ESP32-P4 communication manager
//  Sends JSON commands to C6 and C5 co-processors
//  Receives JSON result streams from each
//
//  Protocol: newline-delimited JSON over UART
//  Each message: {"cmd":"<type>","data":{...}}\n
// ============================================================

#include "comms_manager.h"
#include "../pins.h"
#include "../hawk/hawk_pet.h"
#include <ArduinoJson.h>

// ── Target IDs ────────────────────────────────────────────────────────────────
#define TARGET_C6   0
#define TARGET_C5   1

// ── State ─────────────────────────────────────────────────────────────────────
static bool _c6Ready = false;
static bool _c5Ready = false;

static WiFiResultCb  _wifiCb  = nullptr;
static BLEResultCb   _bleCb   = nullptr;
static DroneResultCb _droneCb = nullptr;
static AlertCb       _alertCb = nullptr;

static char _c6Buf[1024];
static int  _c6BufIdx = 0;
static char _c5Buf[1024];
static int  _c5BufIdx = 0;

// ── Ping/detect co-processors ─────────────────────────────────────────────────
static bool pingCoprocessor(HardwareSerial& serial, uint32_t timeoutMs) {
    // Flush any pending data
    while (serial.available()) serial.read();

    // Send ping
    serial.println("{\"cmd\":\"ping\"}");

    uint32_t start = millis();
    String response = "";

    while (millis() - start < timeoutMs) {
        while (serial.available()) {
            char c = serial.read();
            if (c == '\n') {
                // Parse response
                JsonDocument doc;
                if (deserializeJson(doc, response) == DeserializationError::Ok) {
                    if (strcmp(doc["type"] | "", "pong") == 0) {
                        return true;
                    }
                }
                response = "";
            } else {
                response += c;
            }
        }
        delay(10);
    }
    return false;
}

// ── Parse incoming result from co-processor ───────────────────────────────────
static void parseResult(const char* json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    const char* type = doc["type"] | "";

    if (strcmp(type, RESULT_WIFI_NET) == 0 && _wifiCb) {
        WiFiResult r;
        strlcpy(r.ssid,   doc["data"]["ssid"]   | "", sizeof(r.ssid));
        strlcpy(r.bssid,  doc["data"]["bssid"]  | "", sizeof(r.bssid));
        strlcpy(r.auth,   doc["data"]["auth"]   | "", sizeof(r.auth));
        strlcpy(r.vendor, doc["data"]["vendor"] | "", sizeof(r.vendor));
        r.rssi    = doc["data"]["rssi"]    | -127;
        r.channel = doc["data"]["channel"] | 0;
        r.band    = doc["data"]["band"]    | 0;
        _wifiCb(r);
        HawkPet::feed(FEED_WIFI_SCAN, 1);

    } else if (strcmp(type, RESULT_BLE_DEV) == 0 && _bleCb) {
        BLEResult r;
        strlcpy(r.mac,    doc["data"]["mac"]    | "", sizeof(r.mac));
        strlcpy(r.name,   doc["data"]["name"]   | "", sizeof(r.name));
        strlcpy(r.type,   doc["data"]["type"]   | "", sizeof(r.type));
        strlcpy(r.vendor, doc["data"]["vendor"] | "", sizeof(r.vendor));
        r.rssi        = doc["data"]["rssi"]        | -127;
        r.onWatchlist = doc["data"]["watchlist"]   | false;
        _bleCb(r);
        HawkPet::feed(FEED_BLE_SCAN, 1);

    } else if (strcmp(type, RESULT_DRONE) == 0 && _droneCb) {
        DroneResult r;
        strlcpy(r.id,     doc["data"]["id"]     | "", sizeof(r.id));
        strlcpy(r.uaType, doc["data"]["uaType"] | "", sizeof(r.uaType));
        r.droneLat = doc["data"]["lat"]    | 0.0f;
        r.droneLon = doc["data"]["lon"]    | 0.0f;
        r.droneAlt = doc["data"]["alt"]    | 0.0f;
        r.rssi     = doc["data"]["rssi"]   | -127;
        r.source   = doc["data"]["source"] | 0;
        _droneCb(r);
        HawkPet::feed(FEED_SKYSPY, 1);

    } else if (strcmp(type, RESULT_ALERT) == 0 && _alertCb) {
        _alertCb(doc["data"]["msg"] | "", doc["data"]["detail"] | "");
    }
}

// ── Send command to a co-processor ───────────────────────────────────────────
static void sendCmd(HardwareSerial& serial, const char* cmd, JsonObject params) {
    JsonDocument doc;
    doc["cmd"] = cmd;
    if (!params.isNull()) {
        doc["params"] = params;
    }
    serializeJson(doc, serial);
    serial.println();
}

namespace Comms {

void begin() {
    // Init C6 UART (built-in co-processor)
    Serial1.begin(C6_BAUD);
    delay(200);
    _c6Ready = pingCoprocessor(Serial1, C5_DETECT_MS);
    HH_LOGF("[Comms] C6: %s\n", _c6Ready ? "OK" : "not detected");

    // Init C5 UART (optional external co-processor via M5Bus)
    Serial2.begin(C5_BAUD, SERIAL_8N1, C5_UART_RX, C5_UART_TX);
    delay(200);
    _c5Ready = pingCoprocessor(Serial2, C5_DETECT_MS);
    HH_LOGF("[Comms] C5: %s\n", _c5Ready ? "OK (5GHz!)" : "not detected");
}

bool c6Available()       { return _c6Ready; }
bool c5Available()       { return _c5Ready; }
bool dualBandAvailable() { return _c6Ready && _c5Ready; }

void onWiFiResult(WiFiResultCb cb)   { _wifiCb  = cb; }
void onBLEResult(BLEResultCb cb)     { _bleCb   = cb; }
void onDroneResult(DroneResultCb cb) { _droneCb = cb; }
void onAlert(AlertCb cb)             { _alertCb = cb; }

void sendCommand(uint8_t target, const char* cmd, JsonObject params) {
    if (target == TARGET_C6 && _c6Ready) {
        sendCmd(Serial1, cmd, params);
    } else if (target == TARGET_C5 && _c5Ready) {
        sendCmd(Serial2, cmd, params);
    }
}

void stopAll() {
    if (_c6Ready) Serial1.println("{\"cmd\":\"stop\"}");
    if (_c5Ready) Serial2.println("{\"cmd\":\"stop\"}");
}

// ── Poll incoming data from both co-processors ────────────────────────────────
void poll() {
    // Poll C6
    while (_c6Ready && Serial1.available()) {
        char c = Serial1.read();
        if (c == '\n') {
            if (_c6BufIdx > 0) {
                _c6Buf[_c6BufIdx] = '\0';
                parseResult(_c6Buf);
                _c6BufIdx = 0;
            }
        } else if (_c6BufIdx < (int)sizeof(_c6Buf) - 1) {
            _c6Buf[_c6BufIdx++] = c;
        }
    }

    // Poll C5
    while (_c5Ready && Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            if (_c5BufIdx > 0) {
                _c5Buf[_c5BufIdx] = '\0';
                parseResult(_c5Buf);
                _c5BufIdx = 0;
            }
        } else if (_c5BufIdx < (int)sizeof(_c5Buf) - 1) {
            _c5Buf[_c5BufIdx++] = c;
        }
    }
}

// ── High level command helpers ────────────────────────────────────────────────
void startWiFiScan(bool dual) {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["duration"] = 5000;
    params["hidden"]   = true;

    if (_c6Ready) sendCmd(Serial1, CMD_WIFI_SCAN, params);
    if (dual && _c5Ready) sendCmd(Serial2, CMD_WIFI_SCAN, params);
}

void startBLEScan() {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["passive"] = true;
    if (_c6Ready) sendCmd(Serial1, CMD_BLE_SCAN, params);
}

void startWardriving(double lat, double lon) {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["lat"] = lat;
    params["lon"] = lon;
    if (_c6Ready) sendCmd(Serial1, CMD_WARDRIVING, params);
    if (_c5Ready) sendCmd(Serial2, CMD_WARDRIVING, params);
}

void startSkySpy() {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["ble"]  = true;
    params["wifi"] = true;
    if (_c6Ready) sendCmd(Serial1, CMD_SKYSPY, params);
    if (_c5Ready) sendCmd(Serial2, CMD_SKYSPY, params);
}

void startFlockYou() {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    if (_c6Ready) sendCmd(Serial1, CMD_FLOCKYOU, params);
}

void startDeauth(const char* bssid, int channel) {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["bssid"]   = bssid;
    params["channel"] = channel;
    if (_c6Ready) sendCmd(Serial1, CMD_WIFI_DEAUTH, params);
}

void startBeaconSpam(const char* mode) {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["mode"] = mode;
    if (_c6Ready) sendCmd(Serial1, CMD_WIFI_BEACON, params);
}

void startBLESpam(const char* target) {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["target"] = target;
    if (_c6Ready) sendCmd(Serial1, CMD_BLE_SPAM, params);
}

void startEvilPortal(const char* ssid) {
    JsonDocument doc;
    JsonObject params = doc.to<JsonObject>();
    params["ssid"] = ssid;
    if (_c6Ready) sendCmd(Serial1, CMD_EVIL_PORTAL, params);
}

} // namespace Comms
