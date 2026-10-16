#pragma once

#include <Arduino.h>
#include "Config.h"

// =============================================================
// All BLE discovery and connection-related functions.
// The .h file describes WHAT functions other files may call;
// their actual code (HOW they work) is in BLEManager.cpp.
// =============================================================
namespace BLEManager {

void begin();                   // Initialize the ESP32 BLE stack.
void scanAtStartup();           // First 5-second scan (blocking).
bool needsReconnect(uint8_t id); // Is that Slave offline / not ready?
bool connectSlave(uint8_t id);   // Connect safely using its mutex.
void handleRecovery();          // Called often from Arduino loop().

} // namespace BLEManager
