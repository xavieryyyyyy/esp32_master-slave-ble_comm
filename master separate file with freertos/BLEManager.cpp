#include "BLEManager.h"
#include "SharedState.h"

namespace BLEManager {

// Called by the BLE library after an asynchronous scan ends.
// Keep this callback SHORT: it runs in a Bluetooth-related context.
// We only signal Arduino loop() to process the results later.
static void onScanComplete(BLEScanResults results) {
  State::scanFinished.store(true);
}

// Did an advertisement belong to the selected Slave?
static bool matchesSlave(BLEAdvertisedDevice &device, uint8_t id) {
  String name = device.getName();
  String address = device.getAddress().toString();

  if (id == Config::SLAVE_1) {
    return name == Config::SLAVE1_NAME || address == Config::SLAVE1_ADDRESS;
  }
  return name == Config::SLAVE2_NAME || address == Config::SLAVE2_ADDRESS;
}

// Parse all scan results and save each recognized Slave's advertisement.
// We call this only when a scan has finished, with no reconnect worker
// currently reading State::devices or State::found.
static void saveDiscoveredSlaves(BLEScanResults *results) {
  State::found[0] = false;
  State::found[1] = false;

  if (results == nullptr) {
    Serial.println("No scan results available.");
    return;
  }

  Serial.print("Devices found: ");
  Serial.println(results->getCount());

  for (int i = 0; i < results->getCount(); ++i) {
    BLEAdvertisedDevice device = results->getDevice(i);

    // Helpful during debugging, especially when Slave names are empty.
    Serial.print("Device Name: ");
    Serial.println(device.getName());
    Serial.print("BLE Address: ");
    Serial.println(device.getAddress().toString());

    for (uint8_t id = 0; id < Config::SLAVE_COUNT; ++id) {
      if (matchesSlave(device, id)) {
        State::devices[id] = device; // Store a COPY, not a pointer into results.
        State::found[id] = true;
        Serial.printf("Slave %u found!\n", id + 1);
      }
    }
  }
}

void begin() {
  BLEDevice::init(Config::MASTER_NAME);
  BLEScan *scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  Serial.println("Master BLE initialized");
}

void scanAtStartup() {
  // Setup runs before our command tasks are created, so a blocking
  // startup scan is simple and does not interrupt button control.
  Serial.println("Scanning for both Slaves at startup...");
  BLEScanResults *results = BLEDevice::getScan()->start(
      Config::STARTUP_SCAN_SECONDS, false);
  saveDiscoveredSlaves(results);
}

bool needsReconnect(uint8_t id) {
  if (id >= Config::SLAVE_COUNT) return false;

  // Do not wait here: this check runs in loop(), which reads our button.
  // If another task is using this Slave, check again on a later loop.
  if (xSemaphoreTake(State::connectionMutexes[id], 0) != pdTRUE) {
    return false;
  }

  BLEClient *client = State::clients[id];
  bool missing = client == nullptr || !client->isConnected() ||
                 State::characteristics[id] == nullptr;

  xSemaphoreGive(State::connectionMutexes[id]);
  return missing;
}

// Only call this while holding the matching Slave's mutex.
static bool connectUnlocked(uint8_t id) {
  if (!State::found[id]) {
    Serial.printf("Slave %u not discovered!\n", id + 1);
    return false;
  }

  BLEClient *&client = State::clients[id];
  BLERemoteCharacteristic *&characteristic = State::characteristics[id];

  if (client == nullptr) {
    client = BLEDevice::createClient();
    if (client == nullptr) return false;
  }

  if (!client->isConnected()) {
    // A Characteristic from an OLD connection must not be reused.
    characteristic = nullptr;

    Serial.printf("Connecting to Slave %u...\n", id + 1);
    if (!client->connect(&State::devices[id])) {
      Serial.printf("Slave %u connection failed!\n", id + 1);
      return false;
    }
    Serial.printf("Connected to Slave %u!\n", id + 1);
  }

  if (characteristic != nullptr && characteristic->canWrite()) {
    return true; // Already connected and ready.
  }

  // Both Slave sketches expose the same Service and Characteristic UUIDs.
  BLERemoteService *service = client->getService(BLEUUID(Config::SERVICE_UUID));
  if (service == nullptr) {
    Serial.printf("Slave %u Service not found!\n", id + 1);
    return false;
  }

  characteristic = service->getCharacteristic(BLEUUID(Config::CHARACTERISTIC_UUID));
  if (characteristic == nullptr || !characteristic->canWrite()) {
    characteristic = nullptr;
    Serial.printf("Slave %u Characteristic not ready!\n", id + 1);
    return false;
  }

  Serial.printf("Slave %u Characteristic ready!\n", id + 1);
  return true;
}

bool connectSlave(uint8_t id) {
  if (id >= Config::SLAVE_COUNT) return false;

  // This connection may take time, but it runs in setup() or our
  // separate recovery task, NEVER in the button-handling loop.
  if (xSemaphoreTake(State::connectionMutexes[id], pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.printf("Slave %u busy; retry later.\n", id + 1);
    return false;
  }

  bool success = connectUnlocked(id);
  xSemaphoreGive(State::connectionMutexes[id]);
  return success;
}

static void startBackgroundScan() {
  if (State::scanRunning.load()) return;

  State::scanFinished.store(false);
  State::scanRunning.store(true); // Set BEFORE calling start().

  Serial.println("Attempting background BLE reconnection...");
  bool started = BLEDevice::getScan()->start(
      Config::RECONNECT_SCAN_SECONDS, onScanComplete, false);

  if (!started) {
    State::scanRunning.store(false);
    Serial.println("Could not start background scan; will retry.");
  }
}

void handleRecovery() {
  // Part A: Handle completed asynchronous scan, quickly in loop().
  if (State::scanFinished.exchange(false)) {
    saveDiscoveredSlaves(BLEDevice::getScan()->getResults());
    State::scanRunning.store(false);
    State::lastReconnectAttempt = millis();

    // Only request a worker if we found a Slave that needs recovery.
    bool recover1 = State::found[0] && needsReconnect(0);
    bool recover2 = State::found[1] && needsReconnect(1);
    if ((recover1 || recover2) && State::reconnectTaskHandle != nullptr) {
      // Mark busy before waking the task to avoid starting another scan.
      State::reconnectBusy.store(true);
      xTaskNotifyGive(State::reconnectTaskHandle);
      Serial.println("Background reconnection requested!");
    }
  }

  // Part B: If anything is missing, periodically start a NEW scan.
  // The scan call below returns promptly; its callback signals completion.
  if (State::scanRunning.load() || State::reconnectBusy.load()) return;
  if (!needsReconnect(0) && !needsReconnect(1)) return;

  if (millis() - State::lastReconnectAttempt >= Config::RECONNECT_INTERVAL_MS) {
    State::lastReconnectAttempt = millis();
    startBackgroundScan();
  }
}

} // namespace BLEManager
