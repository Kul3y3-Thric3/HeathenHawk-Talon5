<div align="center">

**HEATHENHAWK TALON5**

M5Stack Tab5 Red Team Toolkit
by Kul3y3-Thric3 / Heavens Heathens / ProTechTor

</div>

---

## What is HeathenHawk Talon5?

HeathenHawk Talon5 is a full-featured red team and security research toolkit built for the **M5Stack Tab5** — a 5" touchscreen ESP32-P4 tablet with built-in ESP32-C6 WiFi/BLE coprocessor. Think Flipper Zero — but with a 5-inch touchscreen, dual-band WiFi, and a colorful interactive UI.

---

## Hardware

- Main SoC: ESP32-P4 @ 360MHz, 32MB PSRAM
- Radio: ESP32-C6 (WiFi 6 + BLE 5) via SDIO
- Display: 5" 1280x720 ST7701 capacitive touch
- IMU: BMI270 (auto-rotation)
- Storage: MicroSD
- Ports: Grove Port A (I2C), M5Bus, USB-C
- Power: 2000mAh LiPo, USB-C charging

### Tested Grove Modules
- M5Stack RFID2 Unit (WS1850S, I2C 0x28) — MIFARE Classic + NTAG read/write
- M5Stack NFC Unit (ST25R3916, I2C 0x50) — Full NFC-A/B/F/V + hardware emulation
- M5Stack Unit GPS SMA (AT6668) — Via M5Bus UART G7/G6

---

## Features

### WiFi
- WiFi Scanner — Live rolling scan, OUI lookup, signal strength, SD logging
- Evil Portal — Captive portal with SSID picker, custom SSID keyboard entry, credential capture
- Deauth — 802.11 deauthentication frames
- Beacon Spam — Multi-SSID beacon broadcast
- Wardriving — GPS-tagged network logging to SD

### Bluetooth
- BLE Scanner — Non-blocking scan, watchlist detection, RSSI display
- BLE Spam — Apple Continuity + Google Fast Pair proximity payloads
- Sky Spy — FAA Remote ID (ASTM F3411) detection from drones
- Flock-You — OUI-based detection of Flock Safety, Raven, Rekor, Axon LPR cameras
- Foxhunter — RSSI proximity radar for tracking a target device

### RFID (RFID2 unit required)
- Read MIFARE Classic 1K/4K, MIFARE Ultralight, NTAG2xx
- Auto key attack with expanded dictionary
- Write NDEF (URL/Text) to blank NTAG tags
- Custom URL/Text entry via on-screen keyboard
- Save/load/clone card data
- View raw sector/page data

### NFC (NFC Unit required)
- Full ISO14443-A/B, FeliCa, ISO15693 detection
- NDEF read (URL, Text auto-parsed)
- Write NDEF to blank tags with keyboard entry
- Copy tag data to blank tags
- Hardware tag emulation via ST25R3916 EmulationLayerA
- Save/Load to SD card

### Other
- GPS Info — NMEA parsing, satellite count, coordinate display
- SD Browser — Browse and view SD card contents
- HawkBird — Companion pet that reacts to tool usage
- Settings — Device configuration

---

## Installation

### Requirements
- Arduino IDE 2.x
- M5Stack board package 3.3.8+
- Board: M5Stack Tab5

### Libraries (install via Library Manager)
- M5Unified
- M5GFX
- TinyGPSPlus
- ArduinoJson
- FastLED
- MFRC522_I2C (by kkloesener)
- M5Unit-NFC (by M5Stack)
- M5UnitUnified (by M5Stack)
- M5Utility (by M5Stack)

### Build and Flash
1. Clone this repo
2. Open HeathenHawk_Talon5.ino in Arduino IDE
3. Select board: M5Stack Tab5
4. Select correct COM port
5. Upload

---

## Hardware Wiring

### Grove Port A — Plug and Play
Connect RFID2 or NFC unit directly to Grove Port A. Both use I2C and work immediately.

### GPS via M5Bus
The Unit GPS SMA requires UART. Wire to M5Bus header on the back of the Tab5:

- GPS TX  to  M5Bus Pin 15 (G7)
- GPS RX  to  M5Bus Pin 16 (G6)
- 5V      to  M5Bus Pin 28
- GND     to  M5Bus Pin 1

---

## Legal and Ethical Use

This tool is for authorized security research and educational use only.

- Only use on networks and devices you own or have explicit permission to test
- Deauth and beacon spam may be illegal in your jurisdiction
- RFID/NFC cloning of access cards you do not own is illegal
- The authors are not responsible for misuse

---

## Credits

See CREDITS.md for full acknowledgments.

Key inspirations: ESP32 Marauder, Bruce Firmware, M5Stick-Nemo, Evil-M5Project,
OUI Spy Unified Blue (@colonelpanichacks), furiousMAC, ASTM F3411.

---

## About

HeathenHawk Talon5 is developed by Eye Luk (Kul3y3-Thric3), recording artist,
audio engineer, aspiring cybersecurity student, and founder of ProTechTor — a
cybersecurity education brand built for musicians and creatives.

- Music: heavensheathens.store
- Security: theprotechtor.com (coming soon)
- GitHub: github.com/Kul3y3-Thric3

---

Built with fire by Heavens Heathens / ProTechTor
"Stay curious. Stay dangerous. Stay legal."
