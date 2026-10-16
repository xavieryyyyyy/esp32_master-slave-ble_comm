// =============================================================
// BCA188 LAB 7 - ESP32 MASTER / 2 SLAVES (FreeRTOS)
// Board: Yellow ESP32-WROOM-32, select "ESP32 Dev Module"
// One push button: GPIO 23 to GND
//
// IMPORTANT: Files in this Arduino sketch folder belong to ONE
// program. Arduino compiles all .cpp files and uploads the result
// together. Do NOT upload individual .cpp/.h tabs separately.
//
// How it is organized:
//   Config.h       -> device names, UUIDs, pins, time settings
//   SharedState.*   -> shared BLE objects, queues, mutexes
//   BLEManager.*    -> BLE discovery / connections / retry
//   RTOSTasks.*     -> command tasks and reconnection worker
//   ButtonHandler.* -> button reading and debouncing
// =============================================================

#include "Config.h"
#include "SharedState.h"
#include "BLEManager.h"
#include "RTOSTasks.h"
#include "ButtonHandler.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Create the two command queues and the two BLE mutexes.
  //    BLE connection functions use these mutexes even at startup.
  if (!RTOSTasks::createResources()) {
    Serial.println("Stopping: FreeRTOS resources unavailable.");
    while (true) delay(1000);
  }

  // 2. Set up the button and remember its initial state.
  ButtonHandler::begin();

  // 3. Initialize BLE and look for BOTH Slaves once at startup.
  BLEManager::begin();
  BLEManager::scanAtStartup();

  // 4. Connect separately; if one is absent, still try the other.
  BLEManager::connectSlave(Config::SLAVE_1);
  BLEManager::connectSlave(Config::SLAVE_2);

  // 5. Start the two command tasks and the recovery task.
  if (!RTOSTasks::start()) {
    Serial.println("Stopping: Could not create FreeRTOS tasks.");
    while (true) delay(1000);
  }

  // 6. Synchronize both LEDs to the initial button state.
  //    If one Slave is offline, it receives this state on recovery.
  uint8_t initialState = State::desiredLedState.load();
  RTOSTasks::queueForBoth(initialState);

  Serial.println("Master setup complete!");
}

void loop() {
  // Read physical input and place commands in the two queues.
  // We do NOT perform BLE writes or blocking BLE connects here.
  ButtonHandler::update();

  // Process completed scans and schedule background recovery.
  BLEManager::handleRecovery();

  // Let other FreeRTOS tasks execute; 1 tick is generally ~1 ms.
  vTaskDelay(1);
}
