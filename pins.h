// ============================================================
//  HeathenHawk Talon5 — pins.h
//  M5Stack Tab5 hardware pin assignments
//  ESP32-P4 main processor
//
//  Tab5 uses M5Unified for most hardware abstraction.
//  These are the raw pin assignments for direct access.
// ============================================================

#pragma once
#include <Arduino.h>
#include <M5Unified.h>

// ── DISPLAY (MIPI-DSI, handled by M5Unified) ─────────────────────────────────
// Display is driven by M5GFX via M5Unified — no manual SPI pins needed
// Resolution: 1280x720, IPS, capacitive touch (GT911 or ST7123)
#define T5_SCREEN_W     1280
#define T5_SCREEN_H     720

// ── TOUCH (GT911 / ST7123, handled by M5Unified) ────────────────────────────
// Touch is initialized automatically by M5Unified

// ── IMU (BMI270 via I2C) ─────────────────────────────────────────────────────
// IMU is initialized by M5Unified
// Used for auto-rotation detection (portrait/landscape/cyberdeck modes)

// ── AUDIO ────────────────────────────────────────────────────────────────────
// ES8388 DAC + ES7210 ADC via I2S — handled by M5Unified
// Built-in speaker and dual microphone array

// ── microSD CARD ─────────────────────────────────────────────────────────────
// Built-in microSD slot — handled by M5Unified SD
#define PIN_SD_CS       -1    // Managed by M5Unified internally

// ── CAMERA (SC2356 2MP MIPI-CSI) ─────────────────────────────────────────────
// 2MP front-facing camera — via MIPI-CSI, initialized by M5Unified

// ── RTC (RX8130) ─────────────────────────────────────────────────────────────
// Real-time clock — via I2C, managed by M5Unified

// ── ESP32-C6 CO-PROCESSOR (built-in WiFi/BLE) ────────────────────────────────
// C6 is the wireless co-processor communicating with P4 via internal UART
// WiFi 6 (2.4GHz) + BLE 5.2 + 802.15.4 (Zigbee/Thread)
#define C6_UART_NUM     1
#define C6_BAUD         115200

// ── ESP32-C5 DEVKIT (optional, via M5Bus UART) ───────────────────────────────
// External C5 DevKit for 5GHz dual-band capability
// Connect C5 TX → Tab5 M5Bus RX, C5 RX → Tab5 M5Bus TX
#define C5_UART_NUM     2
#define C5_UART_RX      18    // M5Bus UART2 RX
#define C5_UART_TX      17    // M5Bus UART2 TX
#define C5_BAUD         115200
#define C5_DETECT_MS    2000  // Timeout for C5 detection on boot

// ── M5BUS EXPANSION ──────────────────────────────────────────────────────────
// 30-pin M5Bus header on back of Tab5
// Provides: 5V, 3.3V, GND, I2C, UART, SPI, GPIO
#define M5BUS_I2C_SDA   8
#define M5BUS_I2C_SCL   9
#define M5BUS_UART_TX   17
#define M5BUS_UART_RX   18

// ── GROVE PORT ───────────────────────────────────────────────────────────────
#define GROVE_SDA       8
#define GROVE_SCL       9

// ── GPIO EXPANSION ───────────────────────────────────────────────────────────
// GPIO_EXT connector on Tab5 side panel
#define GPIO_EXT_1      40
#define GPIO_EXT_2      41
#define GPIO_EXT_3      42
#define GPIO_EXT_4      43

// ── DISPLAY ORIENTATION ──────────────────────────────────────────────────────
// Orientation modes — set based on IMU or user preference
#define ORIENT_PORTRAIT     0   // Normal tablet mode (720x1280)
#define ORIENT_LANDSCAPE    1   // Wide mode / cyberdeck mode (1280x720)
#define ORIENT_PORTRAIT_INV 2   // Inverted portrait
#define ORIENT_LANDSCAPE_INV 3  // Inverted landscape

// ── BOARD CAPABILITY FLAGS ───────────────────────────────────────────────────
#define HH_HAS_TOUCH        1
#define HH_HAS_CAMERA       1
#define HH_HAS_IMU          1
#define HH_HAS_SPEAKER      1
#define HH_HAS_MIC          1
#define HH_HAS_RTC          1
#define HH_HAS_SD           1
#define HH_DUAL_BAND        0   // Set to 1 when C5 detected at runtime
#define BOARD_NAME          "HeathenHawk Talon5"

// ── SERIAL DEBUG ─────────────────────────────────────────────────────────────
#ifdef DEBUG_SERIAL
    #define HH_LOG(x)       Serial.println(x)
    #define HH_LOGF(...)    Serial.printf(__VA_ARGS__)
#else
    #define HH_LOG(x)       do {} while(0)
    #define HH_LOGF(...)    do {} while(0)
#endif
