# Credits & Acknowledgements

HeathenHawk Talon5 is an original firmware built from scratch by
**Kul3y3-Thric3 / Heavens Heathens / Heathen House Ent. / ProTechTor**

We believe strongly in giving credit where it is due. This project would
not exist without the foundational work of the developers and researchers
listed below. Their contributions to the open-source ESP32 security
community made this firmware possible.

---

## Foundational Firmware Projects

### ESP32 Marauder — @justcallmekoko
**https://github.com/justcallmekoko/ESP32Marauder**

The original ESP32 WiFi/Bluetooth offensive security toolkit and the
project that proved a standalone ESP32 device could be a serious red team
tool. The WiFi scanner, deauth, beacon spam, and evil portal concepts in
HeathenHawk all trace their conceptual lineage to Marauder. justcallmekoko
laid the foundation that everything in this ecosystem is built on.

### Bruce Firmware — @pr3y, @bmorcelli, @IncursioHack and contributors
**https://github.com/brucedevices/firmware**

The most feature-complete and actively maintained ESP32 red team firmware.
Bruce was the primary reference for what a full-featured standalone security
research device should look like. @bmorcelli in particular has pushed the
entire ESP32 security firmware ecosystem forward enormously through both
Bruce and the M5 Launcher. Distributed under AGPL-3.0.

### M5Stick-Nemo — @n0xa
**https://github.com/n0xa/m5stick-nemo**

Clean, focused multi-tool firmware for M5Stick devices. Nemo's philosophy
of clean standalone operation without requiring a phone was a direct
inspiration for the HeathenHawk standalone device design approach.

### Evil-M5Project — @7h30th3r0n3
**https://github.com/7h30th3r0n3/Evil-M5Project**

WiFi attack suite for M5Stack. The evil portal captive portal approach
and credential capture flow concepts draw from this project's well-designed
implementation.

### OUI Spy Unified Blue — @colonelpanichacks
**https://github.com/colonelpanichacks/oui-spy-unified-blue**

This is where significant direct credit is owed. The **Flock-You** mode
in HeathenHawk is directly inspired by and builds upon the surveillance
camera detection work pioneered by @colonelpanichacks in OUI Spy. The
concept of passively detecting Flock Safety, Raven, Rekor, and Axon cameras
by their BLE and WiFi OUI signatures — the MAC prefixes, the signature
database approach, the watchlist alerting — this is OUI Spy's original
contribution to the ecosystem. We reimplemented it from scratch in our
own codebase but the detection intelligence, the idea, and the research
behind it belongs to @colonelpanichacks. If Flock-You is useful to you,
go star OUI Spy.

### M5 Launcher — @bmorcelli
**https://github.com/bmorcelli/Launcher**

The multi-firmware launcher and bootloader for ESP32 devices. The Launcher
ecosystem made distributing and switching between firmwares practical for
end users across the entire ESP32 community.

---

## Research & Protocol Work

### FAA Remote ID / ASTM F3411 Detection — Community Research
The **Sky Spy** drone detection mode is based on the FAA Remote ID
standard (ASTM F3411-22a) and the community research that reverse-engineered
how Remote ID broadcasts are structured over BLE advertisements and WiFi
NAN frames. The Marauder and Bruce communities were among the first to
implement Remote ID detection on ESP32 hardware, and their public
documentation of the approach informed our implementation. The ASTM F3411
standard itself is publicly available from ASTM International.

### Apple Continuity Protocol — furiousMAC / @furiousMAC
The BLE scanner's Apple device decoder — identifying FindMy, AirDrop,
AirPlay, Handoff, and other Apple Continuity messages from raw BLE
advertisement data — is based on the reverse engineering work documented
by the furiousMAC project. Their public documentation of the Continuity
protocol type codes made this possible.
**https://github.com/furiousMAC/continuity**

### Wigle.net
The wardriving mode generates CSV files compatible with the Wigle.net
format and supports direct upload to their API. Wigle has maintained
the world's largest wireless network database for over 20 years and
their open API and CSV format specification made integration straightforward.
**https://wigle.net**

---

## Libraries & Frameworks

| Library | Author | Purpose |
|---|---|---|
| M5Unified | M5Stack / @lovyan03 | Tab5 hardware abstraction layer |
| M5GFX / LovyanGFX | @lovyan03 | Display driver framework |
| NimBLE-Arduino | @h2zero | BLE stack for co-processor comms |
| TinyGPSPlus | @mikalhart | GPS NMEA sentence parsing |
| ArduinoJson | @bblanchon | JSON protocol for co-processor comms |
| FastLED | FastLED contributors | NeoPixel/LED control |
| pioarduino | pioarduino community | ESP32-P4 PlatformIO platform support |
| Arduino-ESP32 | Espressif Systems | ESP32 Arduino core framework |

---

## A Note on Originality

All HeathenHawk and HeathenHawk Talon5 code was written from scratch —
no code was copied or forked from any project listed above. The credits
above acknowledge conceptual inspiration, detection intelligence, protocol
research, and the community knowledge base that made this firmware possible.

What we believe are genuine original contributions to the ecosystem:

- **Native ESP32-C5 dual-band firmware** — the first complete red team
  firmware written natively for the ESP32-C5 with WiFi 6 dual-band support
- **ESP32-P4 Tab5 firmware** — the first serious red team firmware for the
  M5Stack Tab5, targeting the ESP32-P4 platform
- **Dual-chip P4 + C6 + C5 architecture** — orchestrating multiple radio
  co-processors from a central P4 processor over a JSON UART protocol
- **Auto-rotating tablet/cyberdeck UI** — IMU-driven portrait/landscape
  switching with separate touch and keyboard layout modes
- **HawkBird tamagotchi companion** — original gamification layer that
  rewards security research activity with an evolving companion
- **Integrated Flock-You + Sky Spy combo** — combining ALPR camera
  detection and drone Remote ID detection in a single field toolkit

---

*Built with 🦅 by Kul3y3-Thric3*
*Heavens Heathens / Heathen House Ent. / ProTechTor*
*github.com/Kul3y3-Thric3*
