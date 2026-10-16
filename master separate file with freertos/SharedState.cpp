#include "SharedState.h"

// =============================================================
// Single definitions of all shared project variables.
// Keeping them here avoids 'multiple definition' linker errors.
// =============================================================
namespace State {

BLEClient *clients[Config::SLAVE_COUNT] = {nullptr, nullptr};
BLERemoteCharacteristic *characteristics[Config::SLAVE_COUNT] = {nullptr, nullptr};

BLEAdvertisedDevice devices[Config::SLAVE_COUNT];
bool found[Config::SLAVE_COUNT] = {false, false};

QueueHandle_t commandQueues[Config::SLAVE_COUNT] = {nullptr, nullptr};
SemaphoreHandle_t connectionMutexes[Config::SLAVE_COUNT] = {nullptr, nullptr};

TaskHandle_t reconnectTaskHandle = nullptr;

std::atomic<uint8_t> desiredLedState{0};
std::atomic<bool> scanFinished{false};
std::atomic<bool> scanRunning{false};
std::atomic<bool> reconnectBusy{false};

unsigned long lastReconnectAttempt = 0;

} // namespace State
