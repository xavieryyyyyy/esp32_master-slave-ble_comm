#pragma once

#include <Arduino.h>

// =============================================================
// BCA188 Lab 7 - ESP32 BLE Master: hardware and project settings
// Change settings here instead of searching through every file.
// =============================================================
namespace Config {

// Both Slave servers advertise this same Service and Characteristic.
static constexpr char SERVICE_UUID[] = "ec8817e4-164d-4c5e-8f2f-9b238a1b4e23";
static constexpr char CHARACTERISTIC_UUID[] = "e70df770-9853-4704-bd6d-d18b3f8dfdb3";

static constexpr char MASTER_NAME[] = "LAB7_MASTER";
static constexpr char SLAVE1_NAME[] = "LAB7_SLAVE_1";   // Black ESP32-WROOM-32
static constexpr char SLAVE2_NAME[] = "LAB7_SLAVE_2";   // Blue ESP32-S3

// MAC-address fallbacks for when a scan result has an empty name.
static constexpr char SLAVE1_ADDRESS[] = "3c:8a:1f:d3:a7:0a";
static constexpr char SLAVE2_ADDRESS[] = "a0:85:e3:e0:15:a1";

// One button currently controls both Slave LEDs.
// Wire one button leg to GPIO 23 and the other to GND.
static constexpr uint8_t BUTTON_PIN = 23;
static constexpr uint32_t DEBOUNCE_MS = 50;

// Startup scan may block. Later reconnection scans are asynchronous.
static constexpr uint32_t STARTUP_SCAN_SECONDS = 5;
static constexpr uint32_t RECONNECT_SCAN_SECONDS = 5;
static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

// Index 0 = Slave 1, index 1 = Slave 2.
static constexpr uint8_t SLAVE_COUNT = 2;
static constexpr uint8_t SLAVE_1 = 0;
static constexpr uint8_t SLAVE_2 = 1;

} // namespace Config
