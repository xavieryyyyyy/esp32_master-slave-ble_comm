#pragma once

#include <Arduino.h>
#include "Config.h"

// =============================================================
// Everything related to custom FreeRTOS tasks and command queues.
// The button loop will QUEUE commands, not wait for BLE writes.
// =============================================================
namespace RTOSTasks {

bool createResources(); // Create the queues and mutexes first.
bool start();           // Start the 2 command tasks + recovery task.

// Call this after a button change, or after a Slave reconnects.
// The message is just one byte: 0 = OFF, 1 = ON.
void queueCommand(uint8_t id, uint8_t ledState);
void queueForBoth(uint8_t ledState);

} // namespace RTOSTasks
