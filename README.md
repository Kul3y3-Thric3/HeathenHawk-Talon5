# 🦅 HeathenHawk Talon5

**M5Stack Tab5 Red Team Toolkit**  
*by Kul3y3-Thric3 / Heavens Heathens / ProTechTor*

---

> ⚠️ **Legal Notice:** For authorized security research and educational use only. Never use offensive features without explicit written permission.

---

## What is Talon5?

HeathenHawk Talon5 is the Tab5 companion to [HeathenHawk](https://github.com/Kul3y3-Thric3/HeathenHawk). Same DNA — built from the ground up for the M5Stack Tab5's 5" touchscreen, dual-chip architecture, and cyberdeck-ready form factor.

It's the first serious red team firmware for the Tab5 — and the only one that runs in both **tablet mode** and **cyberdeck mode** with full keyboard support.

---

## Hardware

### M5Stack Tab5
| Component | Details |
|---|---|
| Main processor | ESP32-P4 dual-core RISC-V 400MHz |
| RAM | 32MB PSRAM + 768KB SRAM |
| Flash | 16MB |
| Wireless | ESP32-C6-MINI-1U (WiFi 6 2.4GHz + BLE 5.2) |
| Display | 5" 1280×720 IPS MIPI-DSI capacitive touch |
| Camera | SC2356 2MP MIPI-CSI |
| IMU | BMI270 6-axis |
| Audio | ES8388 DAC + ES7210 ADC, built-in speaker + dual mic |
| Storage | microSD slot |
| RTC | RX8130 |
| Interfaces | USB-C, USB-A, RS485, Grove, M5Bus (30-pin), GPIO_EXT |

### Optional — ESP32-C5 for dual-band 5GHz
Connect an ESP32-C5 DevKit to the M5Bus UART header on the back of the Tab5. Talon5 auto-detects the C5 on boot and enables 5GHz WiFi 6 scanning and Sky Spy dual-band mode.

```
Tab5 M5Bus Pin 17 (UART TX) → ESP32-C5 DevKit RX
Tab5 M5Bus Pin 18 (UART RX) → ESP32-C5 DevKit TX
Tab5 M5Bus 3.3V             → ESP32-C5 DevKit 3V3
Tab5 M5Bus GND              → ESP32-C5 DevKit GND
```

---

## UI Modes

### Tablet Mode (Portrait)
The default mode. Grid layout with large touch targets. Tilt the Tab5 upright and the BMI270 IMU switches to portrait orientation automatically.

### Cyberdeck Mode (Landscape)
When the M5Stack keyboard attachment is connected, or when held in landscape orientation, the UI switches to a sidebar list + detail panel layout optimized for keyboard navigation. Arrow keys scroll the list, Enter launches the mode.

---

## Features

### 📡 WiFi (via ESP32-C6 + optional C5)
- Dual-band scan 2.4GHz + 5GHz (with C5)
- Deauth — targeted deauthentication
- Beacon spam — SSID flooding
- Evil Portal — captive portal with credential capture
- Wardriving — GPS-tagged WiFi + Wigle.net upload

### 📶 BLE (via ESP32-C6)
- Full passive BLE scanner
- Apple Continuity decoder
- Foxhunter — RSSI proximity tracker
- Flock-You — surveillance camera detection
- BLE spam — iOS/Android/Samsung/Windows pairing popups

### 🛸 Passive Sensing
- Sky Spy — FAA Remote ID drone detection
- Flock-You — Flock Safety / Raven / Rekor camera detection

### 📸 Camera
- Live viewfinder on 5" display
- Snapshot to SD card
- QR code decoder

### 📍 GPS
- Hardware GPS via Grove/M5Bus
- Wardriving with Wigle CSV export
- Direct Wigle API upload

### 🦅 HawkBird
Same tamagotchi companion from HeathenHawk — every tool you use feeds your Hawk. Persists across reboots.

---

## Building

```bash
git clone https://github.com/Kul3y3-Thric3/HeathenHawk-Talon5.git
cd HeathenHawk-Talon5
pip install platformio
pio run -e talon5
```

### Flashing
Use the M5Stack Flash Download Tool or PlatformIO upload:

```bash
pio run -e talon5 --target upload
```

---

## Comparison vs HeathenHawk

| Feature | HeathenHawk (C5) | Talon5 (Tab5) |
|---|---|---|
| Display | 1.14" TFT buttons | 5" 1280×720 touch |
| WiFi | Dual-band C5 native | C6 built-in + C5 optional |
| BLE | NimBLE native | C6 co-processor |
| Camera | No | 2MP built-in |
| Audio | Passive buzzer | Speaker + dual mic |
| IMU | No | BMI270 6-axis |
| GPS | External module | External via Grove |
| Keyboard | No | M5Stack attachment |
| Form factor | Pocket device | Tablet / Cyberdeck |
| Battery | External LiPo | Built-in removable |

---

## Roadmap

- [ ] C5 co-processor firmware (handles 5GHz commands from P4)
- [ ] Camera-based QR scanner
- [ ] Audio alerts and voice mode names
- [ ] OTA update via WiFi
- [ ] Web dashboard via hotspot
- [ ] PCB expansion board for GPS + external antenna

---

## Legal

For **authorized security research, penetration testing, and educational use only.**

---

*Built with 🦅 by Kul3y3-Thric3 — Heavens Heathens / Heathen House Ent. / ProTechTor*
