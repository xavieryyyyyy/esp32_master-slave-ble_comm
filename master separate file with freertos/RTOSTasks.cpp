#include "RTOSTasks.h"
#include "SharedState.h"
#include "BLEManager.h"

namespace RTOSTasks {

bool createResources() {
  for (uint8_t id = 0; id < Config::SLAVE_COUNT; ++id) {
    // Length = 1 means we only keep the newest requested LED state.
    // xQueueOverwrite() can replace an older command with a newer one.
    State::commandQueues[id] = xQueueCreate(1, sizeof(uint8_t));

    // A mutex protects one Slave's BLE Client and Characteristic.
    State::connectionMutexes[id] = xSemaphoreCreateMutex();

    if (State::commandQueues[id] == nullptr ||
        State::connectionMutexes[id] == nullptr) {
      Serial.printf("ERROR: Resources for Slave %u failed!\n", id + 1);
      return false;
    }
  }
  Serial.println("Command queues and BLE mutexes created!");
  return true;
}

void queueCommand(uint8_t id, uint8_t ledState) {
  if (id >= Config::SLAVE_COUNT) return;
  QueueHandle_t queue = State::commandQueues[id];
  if (queue == nullptr) return;

  // Overwrite instead of blocking: the button loop must stay responsive.
  xQueueOverwrite(queue, &ledState);
}

void queueForBoth(uint8_t ledState) {
  queueCommand(Config::SLAVE_1, ledState);
  queueCommand(Config::SLAVE_2, ledState);
}

// One worker for ONE Slave. The parameter tells it which Slave (0 or 1).
// These tasks are independent, so a timeout talking to Slave 1 does not
// directly block the task writing to Slave 2.
static void commandWorker(void *parameter) {
  uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(parameter));
  uint8_t queuedState;

  for (;;) {
    // Sleep until the button or reconnection worker places a command.
    if (xQueueReceive(State::commandQueues[id], &queuedState,
                      portMAX_DELAY) != pdTRUE) {
      continue;
    }

    // The other task might be reconnecting this Slave. Wait for its
    // mutex without locking the button loop or the OTHER Slave's mutex.
    if (xSemaphoreTake(State::connectionMutexes[id], portMAX_DELAY) != pdTRUE) {
      continue;
    }

    BLEClient *client = State::clients[id];
    BLERemoteCharacteristic *characteristic = State::characteristics[id];

    if (client != nullptr && client->isConnected() &&
        characteristic != nullptr && characteristic->canWrite()) {
      // Read the LATEST physical-button state immediately before sending.
      // This prevents an older queued recovery command from overriding a
      // newer button press/release that happened during reconnection.
      uint8_t latestState = State::desiredLedState.load();
      String command = latestState ? "ON" : "OFF";

      // 'true' uses a GATT write WITH response, matching the working
      // Slave firmware (Characteristic has PROPERTY_WRITE).
      // It can wait if that Slave drops power. Only THIS task waits.
      bool success = characteristic->writeValue(command, true);

      Serial.printf("Slave %u Task: %s %s!\n", id + 1,
                    command.c_str(), success ? "sent" : "failed");
    } else {
      Serial.printf("Slave %u Task: Slave offline!\n", id + 1);
    }

    xSemaphoreGive(State::connectionMutexes[id]);
  }
}

// A dedicated worker for potentially slow BLE connection operations.
// It wakes only when BLEManager asks it to reconnect a found Slave.
static void recoveryWorker(void *parameter) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    for (uint8_t id = 0; id < Config::SLAVE_COUNT; ++id) {
      // Do not touch a healthy Slave if only the other one failed.
      if (!State::found[id] || !BLEManager::needsReconnect(id)) continue;

      if (BLEManager::connectSlave(id)) {
        // After a successful reconnect, set this LED to the current
        // button state even if there was no NEW button press.
        uint8_t currentState = State::desiredLedState.load();
        queueCommand(id, currentState);
        Serial.printf("Slave %u LED state synchronization queued!\n", id + 1);
      }
    }

    // All reconnection attempts in this cycle have finished.
    State::reconnectBusy.store(false);
  }
}

bool start() {
  // Two separate command tasks, using the SAME implementation but
  // different IDs, so we don't have to duplicate nearly identical code.
  BaseType_t task1 = xTaskCreatePinnedToCore(
      commandWorker, "Slave1Task", 8192,
      reinterpret_cast<void *>(static_cast<uintptr_t>(Config::SLAVE_1)),
      1, nullptr, 1);

  BaseType_t task2 = xTaskCreatePinnedToCore(
      commandWorker, "Slave2Task", 8192,
      reinterpret_cast<void *>(static_cast<uintptr_t>(Config::SLAVE_2)),
      1, nullptr, 1);

  // This task performs BLE connection attempts outside Arduino loop().
  BaseType_t recovery = xTaskCreatePinnedToCore(
      recoveryWorker, "ReconnectTask", 8192,
      nullptr, 1, &State::reconnectTaskHandle, 1);

  if (task1 != pdPASS || task2 != pdPASS || recovery != pdPASS) {
    Serial.println("ERROR: Failed to start one or more FreeRTOS tasks!");
    return false;
  }

  Serial.println("All FreeRTOS tasks created!");
  return true;
}

} // namespace RTOSTasks
