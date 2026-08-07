// ============================================================
//  HeathenHawk Talon5 — modes/sd_browser.cpp
//  SD card file browser with touch UI
// ============================================================
#include "../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#define MODE_NAME "SD Browser"
#define MAX_FILES 64

struct FileEntry { char name[32]; uint32_t size; bool isDir; };
static FileEntry files[MAX_FILES];
static uint8_t   fileCount  = 0;
static int16_t   selectedIdx = 0;
static int16_t   scrollIdx  = 0;

void loadDir(const char* path) {
    fileCount = 0;
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) return;
    File f = dir.openNextFile();
    while (f && fileCount < MAX_FILES) {
        strlcpy(files[fileCount].name, f.name(), sizeof(files[0].name));
        files[fileCount].size  = f.size();
        files[fileCount].isDir = f.isDirectory();
        fileCount++;
        f.close();
        f = dir.openNextFile();
    }
    dir.close();
}

void renderBrowser(bool sdMounted) {
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME, false, sdMounted, false, 100);

    if (!sdMounted) {
        Display::setTextColor(HH_CORAL, HH_DARK);
        Display::setTextSize(1.6f);
        Display::setCursor(40, Display::height()/2);
        Display::print("No SD card detected");
        return;
    }

    Display::fillRect(0, 48, Display::width(), 36, HH_DARKCARD);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(1.3f);
    Display::setCursor(16, 58);
    char hdr[32]; snprintf(hdr, sizeof(hdr), "%d files on SD card", fileCount);
    Display::print(hdr);

    if (fileCount == 0) {
        Display::setTextColor(HH_GRAY, HH_DARK);
        Display::setCursor(40, Display::height()/2);
        Display::print("SD card is empty");
        return;
    }

    int32_t y = 90;
    int32_t rh = 60;
    for (int16_t i = scrollIdx; i < fileCount && y < Display::height()-40; i++) {
        bool sel = (i == selectedIdx);
        Display::fillRect(0, y, Display::width(), rh, sel ? HH_TEAL : HH_DARKCARD);
        Display::drawLine(0, y+rh-1, Display::width(), y+rh-1, HH_DARK);

        Display::setTextColor(files[i].isDir ? HH_AMBER : HH_WHITE,
                              sel ? HH_TEAL : HH_DARKCARD);
        Display::setTextSize(1.4f);
        Display::setCursor(20, y+10);
        Display::print(files[i].isDir ? "/" : " ");
        Display::print(files[i].name);

        if (!files[i].isDir) {
            Display::setTextColor(sel ? HH_WHITE : HH_GRAY, sel ? HH_TEAL : HH_DARKCARD);
            Display::setTextSize(1.2f);
            Display::setCursor(Display::width()-120, y+10);
            if (files[i].size > 1024*1024)
                Display::printf("%luMB", files[i].size/(1024*1024));
            else if (files[i].size > 1024)
                Display::printf("%luKB", files[i].size/1024);
            else
                Display::printf("%luB", files[i].size);
        }
        y += rh;
    }
}

void viewFile(const char* name) {
    char path[40]; snprintf(path, sizeof(path), "/%s", name);
    File f = SD.open(path, FILE_READ);
    if (!f) { Display::showToast("Cannot open file", HH_CORAL); return; }

    Display::clear(HH_DARK);
    Display::drawStatusBar(name, false, true, false, 100);
    Display::setTextColor(HH_WHITE, HH_DARK);
    Display::setTextSize(1.2f);

    int32_t y = 60;
    while (f.available() && y < Display::height()-50) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            Display::setCursor(16, y);
            char buf[48]; strlcpy(buf, line.c_str(), sizeof(buf));
            Display::print(buf);
            y += 28;
        }
    }
    f.close();

    Display::setTextColor(HH_GRAY, HH_DARK);
    Display::setCursor(16, Display::height()-36);
    Display::print("Tap to go back");
    Display::waitForTap();
}

void mode_sd_browser() {
    bool sdMounted = SD.begin();
    if (sdMounted) loadDir("/");
    renderBrowser(sdMounted);
    selectedIdx = 0; scrollIdx = 0;

    while (true) {
        M5.update();
        auto evt = M5.Touch.getDetail();
        if (evt.wasPressed()) {
            if (evt.x < 30) break;
            if (evt.y >= 90) {
                int16_t idx = scrollIdx + (evt.y - 90) / 60;
                if (idx < fileCount) {
                    if (idx == selectedIdx && !files[idx].isDir) {
                        viewFile(files[idx].name);
                        renderBrowser(sdMounted);
                    } else {
                        selectedIdx = idx;
                        renderBrowser(sdMounted);
                    }
                }
            }
        }

        static int32_t lastY = 0;
        if (evt.isPressed()) {
            if (lastY > 0) {
                int32_t dy = lastY - evt.y;
                if (abs(dy) > 20) {
                    scrollIdx = constrain(scrollIdx + (dy>0?1:-1), 0, max(0, fileCount-8));
                    renderBrowser(sdMounted);
                    lastY = evt.y;
                }
            } else lastY = evt.y;
        } else lastY = 0;

        HawkPet::tick(); delay(16);
    }
}
