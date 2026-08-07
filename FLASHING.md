# Flashing HeathenHawk Talon5

## Requirements
- M5Stack Tab5
- USB-C data cable (not charge-only)
- Chrome or Edge browser (for WebSerial flashing)
- The firmware binary from [Releases](https://github.com/Kul3y3-Thric3/HeathenHawk-Talon5/releases)

---

## Method 1 — Web Flasher (Recommended, no software needed)

1. Download `HeathenHawk_Talon5_vX.X.X.bin` from the
   [Releases page](https://github.com/Kul3y3-Thric3/HeathenHawk-Talon5/releases)

2. Open **Chrome or Edge** and go to:
   **https://adafruit.github.io/Adafruit_WebSerial_ESPTool/**

3. Put your Tab5 into download mode:
   - Press and hold the **Reset** button for ~2 seconds
   - Release when the internal green LED starts flashing quickly
   - The device is now in download mode

4. Click **Connect** in the web flasher and select your Tab5's port
   (usually shows as `USB JTAG/serial debug unit`)

5. Set the flash address to `0x0`

6. Click the file picker and select the downloaded `.bin` file

7. Click **Program** and wait for the progress bar to complete

8. The Tab5 will automatically reset and boot into HeathenHawk Talon5

---

## Method 2 — esptool (Command line)

```bash
pip install esptool

esptool --chip esp32p4 \
        --port /dev/ttyUSB0 \
        --baud 460800 \
        write-flash 0x0 HeathenHawk_Talon5_vX.X.X.bin
```

Replace `/dev/ttyUSB0` with your actual port:
- **Mac:** `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`
- **Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`
- **Windows:** `COM3` (or whichever COM port appears)

---

## Method 3 — Build from Source

```bash
git clone https://github.com/Kul3y3-Thric3/HeathenHawk-Talon5.git
cd HeathenHawk-Talon5
pip install platformio
pio run -e talon5
```

Flash the compiled binary:
```bash
cp .pio/build/talon5/firmware.factory.bin ~/Desktop/HeathenHawk_Talon5.bin
```

Then flash via Method 1 or Method 2 above.

---

## Optional — ESP32-C5 DevKit for Dual-Band 5GHz

To enable dual-band 5GHz scanning, connect an ESP32-C5 DevKit to the
Tab5's M5Bus header on the back of the device:

| Tab5 M5Bus Pin | ESP32-C5 DevKit |
|---|---|
| Pin 17 (UART TX) | RX pin |
| Pin 18 (UART RX) | TX pin |
| 3.3V | 3V3 |
| GND | GND |

The C5 DevKit needs to be flashed with the HeathenHawk C5 co-processor
firmware (coming soon). Once connected, Talon5 auto-detects the C5 on boot
and enables dual-band mode — the status bar will show a `5GHz` badge.

Without the C5, everything works normally on 2.4GHz via the built-in C6.

---

## First Boot

After flashing:

1. The Tab5 will display the HeathenHawk Talon5 boot animation
2. You'll hear 4 ascending tones through the built-in speaker
3. If a C5 DevKit is connected, you'll see a toast: `C5 detected — Dual-band active!`
4. The main touch menu appears — tap any mode to launch it

**Tablet mode:** Hold the Tab5 upright (portrait) for the grid touch menu

**Cyberdeck mode:** Rotate to landscape or attach the M5Stack keyboard
for the sidebar list layout with keyboard navigation

---

## Settings & Configuration

To configure Wigle credentials, WiFi upload settings, or HawkBird name:

1. Create a file called `config.txt` on your microSD card (FAT32)
2. Add your settings:
```
WIGLE_USER=yourusername
WIGLE_KEY=yourapikey
UPLOAD_SSID=YourHomeWiFi
UPLOAD_PASS=yourpassword
HAWK_NAME=Talon
```
3. Insert the SD card, open **Settings** from the main menu
4. Tap **Load config.txt** — settings load automatically and the file
   is deleted for security

---

## Troubleshooting

**Tab5 not detected by web flasher:**
- Make sure you're using Chrome or Edge (not Safari)
- Try a different USB-C cable — some are charge-only
- Check that the Tab5 is in download mode (green LED flashing)

**Flash fails at 0%:**
- Put the Tab5 back into download mode and try again immediately
- Lower the baud rate in esptool: `--baud 115200`

**Blank screen after flashing:**
- The firmware is likely running but the display needs a moment
- Try pressing the Reset button once briefly
- If still blank, reflash with the `firmware.factory.bin` which
  includes the bootloader

**C5 not detected:**
- Check wiring — TX/RX must be crossed (Tab5 TX → C5 RX)
- Make sure C5 DevKit is powered (3.3V connected)
- C5 co-processor firmware required (coming soon)

---

*For support: https://github.com/Kul3y3-Thric3/HeathenHawk-Talon5/issues*
