#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include "Config.h"

// =============================================================
// Variables used by MORE THAN ONE .cpp file live here.
// 'extern' is a declaration, NOT a second copy of the variable.
// The actual variables are defined once in SharedState.cpp.
// =============================================================
namespace State {

// One BLE Client and Characteristic per Slave.
extern BLEClient *clients[Config::SLAVE_COUNT];
extern BLERemoteCharacteristic *characteristics[Config::SLAVE_COUNT];

// Advertisements saved by scanning, for later connection attempts.
extern BLEAdvertisedDevice devices[Config::SLAVE_COUNT];
extern bool found[Config::SLAVE_COUNT];

// One latest-command queue and one mutex per Slave.
// The mutex prevents a reconnect and a BLE write to the SAME Slave
// from manipulating its client connection at the same time.
extern QueueHandle_t commandQueues[Config::SLAVE_COUNT];
extern SemaphoreHandle_t connectionMutexes[Config::SLAVE_COUNT];

// The FreeRTOS worker handles potentially slow BLE connections.
extern TaskHandle_t reconnectTaskHandle;

// Atomic values can safely be read/written across different tasks.
// desiredLedState: 0 = OFF, 1 = ON, based on the physical button.
extern std::atomic<uint8_t> desiredLedState;
extern std::atomic<bool> scanFinished;
extern std::atomic<bool> scanRunning;
extern std::atomic<bool> reconnectBusy;

// Used only by Arduino's loop() to schedule retries.
extern unsigned long lastReconnectAttempt;

} // namespace State
