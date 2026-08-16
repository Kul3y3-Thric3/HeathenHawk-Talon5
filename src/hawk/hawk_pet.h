// ============================================================
//  HeathenHawk Talon5 — hawk/hawk_pet.h
//  HawkBird tamagotchi companion
//  Upgraded for Tab5: bigger sprites, real audio, camera feed
// ============================================================

#pragma once
#include <Arduino.h>

// ── Feed types ────────────────────────────────────────────────────────────────
#define FEED_WIFI_SCAN      0
#define FEED_BLE_SCAN       1
#define FEED_WARDRIVING     2
#define FEED_DEAUTH         3
#define FEED_BEACON_SPAM    4
#define FEED_BLE_SPAM       5
#define FEED_EVIL_PORTAL    6
#define FEED_FOXHUNTER      7
#define FEED_SKYSPY         8
#define FEED_FLOCKYOU       9
#define FEED_GPS            10
#define FEED_SD_LOG         11
#define FEED_CAMERA         12
#define FEED_RFID           13
#define FEED_NFC            14
#define FEED_COUNT          15

// ── Evolution stages ──────────────────────────────────────────────────────────
enum HawkStage {
    STAGE_EGG       = 0,
    STAGE_CHICK     = 1,
    STAGE_FLEDGLING = 2,
    STAGE_HUNTER    = 3,
    STAGE_APEX      = 4,
    STAGE_COUNT     = 5
};

namespace HawkPet {
    void        begin();
    void        tick();
    void        feed(uint8_t feedType, uint8_t amount = 1);
    void        drawHawkScreen();
    void        drawHawkWidget(int32_t x, int32_t y, int32_t size);
    void        reset();
    void        setName(const char* name);
    const char* getName();
    const char* getStageName();
    uint32_t    getXP();
    HawkStage   getStage();
    uint8_t     getHunger();
    uint8_t     getHappiness();
}
