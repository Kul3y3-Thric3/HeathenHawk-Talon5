// ============================================================
//  HeathenHawk Talon5 — hawk/hawk_pet.cpp
//  HawkBird tamagotchi — Tab5 edition
//  Bigger sprites on 5" display, real speaker audio
// ============================================================

#include "hawk_pet.h"
#include "../display/display_driver.h"
#include "../../pins.h"
#include <Preferences.h>
#include <M5Unified.h>

// ── XP thresholds per stage ───────────────────────────────────────────────────
static const uint32_t STAGE_XP[] = { 0, 100, 500, 1500, 5000 };

// ── XP per feed type ──────────────────────────────────────────────────────────
static const uint8_t FEED_XP[] = {
    2,   // FEED_WIFI_SCAN
    2,   // FEED_BLE_SCAN
    5,   // FEED_WARDRIVING
    1,   // FEED_DEAUTH
    1,   // FEED_BEACON_SPAM
    1,   // FEED_BLE_SPAM
    8,   // FEED_EVIL_PORTAL
    3,   // FEED_FOXHUNTER
    5,   // FEED_SKYSPY
    4,   // FEED_FLOCKYOU
    3,   // FEED_GPS
    2,   // FEED_SD_LOG
    4,   // FEED_CAMERA
};

static const char* STAGE_NAMES[] = {
    "Egg", "Chick", "Fledgling", "Hunter", "Apex Predator"
};

static const char* FEED_NAMES[] = {
    "WiFi packet", "BLE frame", "GPS waypoint", "Deauth frame",
    "Beacon", "BLE popup", "Credential", "RSSI ping",
    "Remote ID", "ALPR sighting", "GPS fix", "Log entry", "Camera snap"
};

// ── State ─────────────────────────────────────────────────────────────────────
struct HawkState {
    uint32_t   xp;
    uint8_t    hunger;
    uint8_t    happiness;
    HawkStage  stage;
    char       name[20];
    uint32_t   lastFeedMs;
    uint32_t   lastTickMs;
    uint32_t   totalFeeds;
};

static HawkState state;
static bool initialized = false;

// ── Persistence ───────────────────────────────────────────────────────────────
static void saveState() {
    Preferences prefs;
    prefs.begin("hawkpet", false);
    prefs.putUInt("xp",        state.xp);
    prefs.putUChar("hunger",   state.hunger);
    prefs.putUChar("happy",    state.happiness);
    prefs.putUChar("stage",    (uint8_t)state.stage);
    prefs.putString("name",    state.name);
    prefs.putUInt("feeds",     state.totalFeeds);
    prefs.end();
}

static void loadState() {
    Preferences prefs;
    prefs.begin("hawkpet", true);
    state.xp         = prefs.getUInt("xp",        0);
    state.hunger     = prefs.getUChar("hunger",    100);
    state.happiness  = prefs.getUChar("happy",     100);
    state.stage      = (HawkStage)prefs.getUChar("stage", 0);
    state.totalFeeds = prefs.getUInt("feeds",      0);
    prefs.getString("name", state.name, sizeof(state.name));
    prefs.end();
    if (strlen(state.name) == 0) strlcpy(state.name, "Talon", sizeof(state.name));
}

// ── Evolution check ───────────────────────────────────────────────────────────
static void checkEvolution() {
    if (state.stage >= STAGE_APEX) return;
    HawkStage next = (HawkStage)((uint8_t)state.stage + 1);
    if (state.xp >= STAGE_XP[next]) {
        state.stage = next;

        // Evolution celebration — speaker melody
        M5.Speaker.setVolume(200);
        uint16_t notes[] = {523, 659, 784, 1047, 1319, 1568};
        for (auto n : notes) {
            M5.Speaker.tone(n, 120);
            delay(130);
        }
        M5.Speaker.stop();

        // Flash display
        for (int i = 0; i < 4; i++) {
            Display::clear(HH_AMBER);
            delay(80);
            Display::clear(HH_DARK);
            delay(80);
        }

        Display::showAlert("EVOLUTION!", STAGE_NAMES[state.stage], HH_AMBER, 2500);
        saveState();
    }
}

// ── Draw hawk sprite — large version for 5" display ──────────────────────────
static void drawHawkSprite(int32_t cx, int32_t cy, HawkStage stage,
                           uint32_t wingColor, bool wingUp) {
    // Scale up significantly for the 5" display
    // Each pixel block is 8x8 for a nice big sprite

    switch (stage) {
        case STAGE_EGG: {
            // Large egg
            Display::fillCircle(cx, cy, 60, HH_TEAL);
            Display::drawCircle(cx, cy, 60, HH_WHITE);
            // Crack lines
            Display::drawLine(cx-10, cy-30, cx, cy-10, HH_WHITE);
            Display::drawLine(cx, cy-10, cx+15, cy-25, HH_WHITE);
            // Eye peeking out
            Display::fillCircle(cx-15, cy-10, 8, HH_WHITE);
            Display::fillCircle(cx-15, cy-10, 4, HH_DARK);
            break;
        }
        case STAGE_CHICK: {
            // Round fluffy body
            Display::fillCircle(cx, cy+20, 50, HH_AMBER);
            // Head
            Display::fillCircle(cx, cy-30, 35, HH_AMBER);
            // Eyes
            Display::fillCircle(cx-12, cy-38, 10, HH_WHITE);
            Display::fillCircle(cx+12, cy-38, 10, HH_WHITE);
            Display::fillCircle(cx-12, cy-38, 5,  HH_DARK);
            Display::fillCircle(cx+12, cy-38, 5,  HH_DARK);
            // Beak
            Display::fillCircle(cx, cy-22, 8, HH_CORAL);
            // Tiny wings
            Display::fillCircle(cx-52, cy+10, 20, wingColor);
            Display::fillCircle(cx+52, cy+10, 20, wingColor);
            // Feet
            Display::fillRect(cx-20, cy+65, 12, 20, HH_CORAL);
            Display::fillRect(cx+8,  cy+65, 12, 20, HH_CORAL);
            break;
        }
        case STAGE_FLEDGLING: {
            // Sleeker body
            Display::fillRoundRect(cx-35, cy-20, 70, 90, 20, wingColor);
            // Head
            Display::fillCircle(cx, cy-40, 38, HH_GRAY);
            // Eyes
            Display::fillCircle(cx-14, cy-48, 10, HH_WHITE);
            Display::fillCircle(cx+14, cy-48, 10, HH_WHITE);
            Display::fillCircle(cx-14, cy-48, 5,  HH_DARK);
            Display::fillCircle(cx+14, cy-48, 5,  HH_DARK);
            // Eye shine
            Display::fillCircle(cx-12, cy-50, 2, HH_WHITE);
            Display::fillCircle(cx+16, cy-50, 2, HH_WHITE);
            // Beak
            Display::fillRoundRect(cx-6, cy-30, 12, 14, 4, HH_AMBER);
            // Wings spread
            int32_t wingY = wingUp ? cy : cy+15;
            Display::fillRoundRect(cx-100, wingY-15, 70, 40, 12, wingColor);
            Display::fillRoundRect(cx+30,  wingY-15, 70, 40, 12, wingColor);
            // Talons
            Display::fillRect(cx-20, cy+65, 10, 24, HH_AMBER);
            Display::fillRect(cx+10, cy+65, 10, 24, HH_AMBER);
            break;
        }
        case STAGE_HUNTER: {
            // Powerful hawk body
            Display::fillRoundRect(cx-40, cy-25, 80, 100, 18, wingColor);
            // White chest
            Display::fillRoundRect(cx-25, cy+10, 50, 60, 12, HH_WHITE);
            // Head with hood
            Display::fillCircle(cx, cy-50, 42, HH_DARK);
            Display::fillCircle(cx, cy-44, 36, wingColor);
            // Sharp eyes
            Display::fillCircle(cx-16, cy-56, 12, HH_AMBER);
            Display::fillCircle(cx+16, cy-56, 12, HH_AMBER);
            Display::fillCircle(cx-16, cy-56, 6,  HH_DARK);
            Display::fillCircle(cx+16, cy-56, 6,  HH_DARK);
            Display::fillCircle(cx-14, cy-58, 2,  HH_WHITE);
            Display::fillCircle(cx+18, cy-58, 2,  HH_WHITE);
            // Hooked beak
            Display::fillRoundRect(cx-8, cy-36, 16, 18, 4, HH_AMBER);
            Display::fillRect(cx-4, cy-20, 14, 8, HH_AMBER);
            // Large wings
            int32_t wingY = wingUp ? cy-20 : cy;
            Display::fillRoundRect(cx-130, wingY-20, 95, 50, 16, wingColor);
            Display::fillRoundRect(cx+35,  wingY-20, 95, 50, 16, wingColor);
            // Wing tips — darker
            Display::fillRoundRect(cx-130, wingY-10, 30, 35, 8, HH_DARK);
            Display::fillRoundRect(cx+100, wingY-10, 30, 35, 8, HH_DARK);
            // Talons
            Display::fillRect(cx-25, cy+70, 14, 28, HH_AMBER);
            Display::fillRect(cx+11, cy+70, 14, 28, HH_AMBER);
            // Claw details
            for (int t = -1; t <= 1; t++) {
                Display::fillRect(cx-25+t*12, cy+95, 6, 10, HH_AMBER);
                Display::fillRect(cx+11+t*12, cy+95, 6, 10, HH_AMBER);
            }
            break;
        }
        case STAGE_APEX: {
            // Apex Predator — full glory
            // Body
            Display::fillRoundRect(cx-45, cy-30, 90, 110, 20, HH_DARK);
            Display::fillRoundRect(cx-30, cy+10, 60, 70, 14, HH_WHITE);
            // Circuit pattern on chest
            Display::drawLine(cx-20, cy+20, cx-20, cy+60, HH_TEAL);
            Display::drawLine(cx+20, cy+20, cx+20, cy+60, HH_TEAL);
            Display::drawLine(cx-20, cy+40, cx+20, cy+40, HH_TEAL);
            Display::fillCircle(cx, cy+40, 6, HH_TEAL);
            // Head
            Display::fillCircle(cx, cy-55, 45, HH_DARK);
            Display::fillCircle(cx, cy-50, 38, HH_GRAY);
            // Glowing eyes
            Display::fillCircle(cx-17, cy-62, 14, HH_TEAL);
            Display::fillCircle(cx+17, cy-62, 14, HH_TEAL);
            Display::fillCircle(cx-17, cy-62, 7,  HH_DARK);
            Display::fillCircle(cx+17, cy-62, 7,  HH_DARK);
            Display::fillCircle(cx-15, cy-65, 3,  HH_WHITE);
            Display::fillCircle(cx+19, cy-65, 3,  HH_WHITE);
            // Crown
            for (int f = -2; f <= 2; f++) {
                Display::fillRect(cx + f*14 - 4, cy-100 + abs(f)*8, 8,
                                  24 - abs(f)*6, HH_AMBER);
            }
            // Massive wings
            int32_t wingY = wingUp ? cy-30 : cy-10;
            Display::fillRoundRect(cx-160, wingY-25, 120, 60, 18, HH_DARK);
            Display::fillRoundRect(cx+40,  wingY-25, 120, 60, 18, HH_DARK);
            // Wing highlights
            Display::fillRoundRect(cx-155, wingY-20, 100, 20, 8, HH_GRAY);
            Display::fillRoundRect(cx+55,  wingY-20, 100, 20, 8, HH_GRAY);
            // Teal circuit wing detail
            Display::drawLine(cx-140, wingY-10, cx-60, wingY-10, HH_TEAL);
            Display::drawLine(cx+60,  wingY-10, cx+140, wingY-10, HH_TEAL);
            // Talons
            for (int t = 0; t < 2; t++) {
                int32_t tx = cx + (t == 0 ? -30 : 16);
                Display::fillRect(tx, cy+75, 14, 30, HH_AMBER);
                for (int c = -1; c <= 1; c++) {
                    Display::fillRect(tx + c*10, cy+102, 6, 12, HH_AMBER);
                }
            }
            break;
        }
    }
}

// ── Hunger/happiness bar ──────────────────────────────────────────────────────
static void drawStatBar(int32_t x, int32_t y, int32_t w, int32_t h,
                        uint8_t val, uint32_t color, const char* label) {
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.3f);
    Display::setCursor(x, y - 22);
    Display::print(label);
    Display::drawRoundRect(x, y, w, h, h/2, HH_GRAY);
    int32_t filled = w * val / 100;
    if (filled > 0) Display::fillRoundRect(x, y, filled, h, h/2, color);
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", val);
    Display::setTextColor(HH_WHITE, HH_DARK);
    Display::setCursor(x + w + 10, y + h/2 - 8);
    Display::print(pct);
}

namespace HawkPet {

void begin() {
    memset(&state, 0, sizeof(state));
    loadState();
    state.lastFeedMs = millis();
    state.lastTickMs = millis();
    initialized = true;
    HH_LOGF("[HawkPet] %s loaded — Stage: %s  XP: %lu\n",
            state.name, STAGE_NAMES[state.stage], state.xp);
}

void tick() {
    if (!initialized) return;
    uint32_t now = millis();

    // Decay every 60 seconds
    if (now - state.lastTickMs > 60000) {
        state.lastTickMs = now;

        if (state.hunger   > 0) state.hunger   -= 2;
        if (state.happiness > 0) state.happiness -= 1;

        // Hungry hawk is unhappy
        if (state.hunger < 20 && state.happiness > 0) state.happiness -= 2;

        state.hunger    = max((uint8_t)0, state.hunger);
        state.happiness = max((uint8_t)0, state.happiness);

        // Save periodically
        saveState();
    }
}

void feed(uint8_t feedType, uint8_t amount) {
    if (!initialized || feedType >= FEED_COUNT) return;

    uint8_t xpGain = FEED_XP[feedType] * amount;
    state.xp         += xpGain;
    state.totalFeeds += amount;
    state.lastFeedMs  = millis();

    // Restore hunger and happiness
    state.hunger    = min(100, state.hunger    + xpGain * 2);
    state.happiness = min(100, state.happiness + xpGain);

    checkEvolution();
}

void drawHawkScreen() {
    Display::clear(HH_DARK);
    Display::drawStatusBar(state.name, false, false, false, 100);

    int32_t cx = Display::width()  / 2;
    int32_t cy = Display::height() / 2 - 40;

    // Animated wing flap
    bool wingUp = (millis() / 600) % 2 == 0;

    // Color per stage
    uint32_t wingColors[] = {
        HH_TEAL, HH_AMBER, HH_GRAY, HH_DARK, HH_DARK
    };

    drawHawkSprite(cx, cy, state.stage, wingColors[state.stage], wingUp);

    // Stage name
    Display::setTextColor(HH_AMBER, HH_DARK);
    Display::setTextSize(2.0f);
    Display::setCursor(cx - 80, 60);
    Display::print(STAGE_NAMES[state.stage]);

    // Name
    Display::setTextColor(HH_WHITE, HH_DARK);
    Display::setTextSize(1.6f);
    Display::setCursor(cx - 60, 96);
    Display::print(state.name);

    // Stats bars
    int32_t barW = Display::width() - 120;
    int32_t barX = 60;
    int32_t barY = Display::height() - 160;

    drawStatBar(barX, barY,      barW, 20, state.hunger,    HH_CORAL,  "HUNGER");
    drawStatBar(barX, barY + 56, barW, 20, state.happiness, HH_PURPLE, "HAPPY");

    // XP progress to next stage
    if (state.stage < STAGE_APEX) {
        uint32_t xpNext = STAGE_XP[state.stage + 1];
        uint32_t xpCur  = STAGE_XP[state.stage];
        uint8_t  pct    = (uint8_t)((state.xp - xpCur) * 100 / (xpNext - xpCur));
        drawStatBar(barX, barY + 112, barW, 20, pct, HH_TEAL, "XP TO NEXT STAGE");
    }

    // Stats footer
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.2f);
    Display::setCursor(barX, barY + 145);
    char buf[48];
    snprintf(buf, sizeof(buf), "Total XP: %lu   Feeds: %lu", state.xp, state.totalFeeds);
    Display::print(buf);

    // Hint
    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setTextSize(1.2f);
    Display::setCursor(barX, Display::height() - 30);
    Display::print("Tap anywhere or use tools to feed your Hawk");

    // Wait for tap or timeout
    uint32_t showMs = millis();
    while (millis() - showMs < 10000) {
        M5.update();
        if (M5.Touch.getDetail().wasPressed()) break;
        // Reanimate wings
        bool wUp = (millis() / 600) % 2 == 0;
        if (wUp != wingUp) {
            wingUp = wUp;
            drawHawkSprite(cx, cy, state.stage, wingColors[state.stage], wingUp);
        }
        delay(50);
    }
}

void drawHawkWidget(int32_t x, int32_t y, int32_t size) {
    // Mini hawk widget for status display
    bool wingUp = (millis() / 800) % 2 == 0;
    uint32_t wingColors[] = {
        HH_TEAL, HH_AMBER, HH_GRAY, HH_DARK, HH_DARK
    };
    // Scale down the sprite for widget use
    drawHawkSprite(x + size/2, y + size/2,
                   state.stage, wingColors[state.stage], wingUp);
}

void reset() {
    memset(&state, 0, sizeof(state));
    strlcpy(state.name, "Talon", sizeof(state.name));
    state.hunger    = 100;
    state.happiness = 100;
    state.stage     = STAGE_EGG;
    saveState();
    HH_LOG("[HawkPet] Reset to egg");
}

void        setName(const char* name) {
    strlcpy(state.name, name, sizeof(state.name));
    saveState();
}

const char* getName()      { return state.name; }
const char* getStageName() { return STAGE_NAMES[state.stage]; }
uint32_t    getXP()        { return state.xp; }
HawkStage   getStage()     { return state.stage; }
uint8_t     getHunger()    { return state.hunger; }
uint8_t     getHappiness() { return state.happiness; }

} // namespace HawkPet
