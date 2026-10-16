#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>


  // Shared BLE Service and Characteristic UUIDs
  #define SERVICE_UUID "ec8817e4-164d-4c5e-8f2f-9b238a1b4e23"
  #define CHARACTERISTIC_UUID "e70df770-9853-4704-bd6d-d18b3f8dfdb3"

  // Slave device names
  #define SLAVE1_NAME "LAB7_SLAVE_1"
  #define SLAVE2_NAME "LAB7_SLAVE_2"

  // Master push button
  #define BUTTON_PIN 23

  // Remember the latest requested LED state
// 0 = OFF, 1 = ON
volatile uint8_t desiredLedState = 0;

  // Separate BLE Clients
  BLEClient *slave1Client = nullptr;
  BLEClient *slave2Client = nullptr;

  // Separate BLE Characteristics
  BLERemoteCharacteristic *remoteCharacteristic = nullptr;
  BLERemoteCharacteristic *slave2Characteristic = nullptr;
  
  // Independent command queues for each Slave
  QueueHandle_t slave1CommandQueue = nullptr;
  QueueHandle_t slave2CommandQueue = nullptr;

  // Protect each Slave's BLE connection independently
SemaphoreHandle_t slave1Mutex = nullptr;
SemaphoreHandle_t slave2Mutex = nullptr;

// Background BLE reconnection task
TaskHandle_t reconnectTaskHandle = nullptr;

// Track whether reconnection is running
volatile bool reconnectWorkerBusy = false;

  // Automatic BLE reconnection settings
  unsigned long lastReconnectAttempt = 0;

  // Retry disconnected Slaves every 5 seconds
  const unsigned long RECONNECT_INTERVAL = 5000;

  // Track completion of background BLE scanning
  volatile bool reconnectScanFinished = false;

  // Track whether background scanning is running
bool reconnectScanRunning = false;

  // Store discovered BLE devices for reconnection
  BLEAdvertisedDevice slave1Device;
  BLEAdvertisedDevice slave2Device;

  // Track whether each Slave was discovered
  bool foundSlave1 = false;
  bool foundSlave2 = false;

  // Check whether Slave 1 needs reconnection
  bool slave1NeedsReconnect() {
    return slave1Client == nullptr ||
          !slave1Client->isConnected() ||
          remoteCharacteristic == nullptr;
  }

  // Check whether Slave 2 needs reconnection
  bool slave2NeedsReconnect() {
    return slave2Client == nullptr ||
          !slave2Client->isConnected() ||
          slave2Characteristic == nullptr;
  }


  // Send ON or OFF to both Slaves
  void sendCommandToSlaves(String command) {

    // Send command to Slave 1
    if (slave1Client != nullptr &&
        slave1Client->isConnected() &&
        remoteCharacteristic != nullptr &&
        remoteCharacteristic->canWrite()) {

      bool success =
          remoteCharacteristic->writeValue(command, true);

      Serial.print("Slave 1: ");
      Serial.print(command);
      Serial.println(success ? " sent!" : " failed!");

    } else {
      Serial.println("Slave 1 not ready!");
    }

    // Send command to Slave 2
    if (slave2Client != nullptr &&
        slave2Client->isConnected() &&
        slave2Characteristic != nullptr &&
        slave2Characteristic->canWrite()) {

      bool success =
          slave2Characteristic->writeValue(command, true);

      Serial.print("Slave 2: ");
      Serial.print(command);
      Serial.println(success ? " sent!" : " failed!");

    } else {
      Serial.println("Slave 2 not ready!");
    }
  }

 // Independent communication task for Slave 1
void slave1CommandTask(void *parameter) {

  uint8_t ledState;

  while (true) {

    // Wait for a command
    if (xQueueReceive(
          slave1CommandQueue,
          &ledState,
          portMAX_DELAY) == pdTRUE) {

      // Lock Slave 1's BLE connection
      if (xSemaphoreTake(slave1Mutex, portMAX_DELAY) == pdTRUE) {

        // Check whether Slave 1 is ready
        if (slave1Client != nullptr &&
            slave1Client->isConnected() &&
            remoteCharacteristic != nullptr &&
            remoteCharacteristic->canWrite()) {

          String command = ledState ? "ON" : "OFF";

          bool success =
              remoteCharacteristic->writeValue(command, true);

          Serial.print("Slave 1 Task: ");
          Serial.print(command);
          Serial.println(success ? " sent!" : " failed!");

        } else {
          Serial.println("Slave 1 Task: Slave offline!");
        }

        // Unlock so other tasks can access Slave 1
        xSemaphoreGive(slave1Mutex);
      }
    }
  }
}

// Independent communication task for Slave 2
void slave2CommandTask(void *parameter) {

  uint8_t ledState;

  while (true) {

    // Wait for a command
    if (xQueueReceive(
          slave2CommandQueue,
          &ledState,
          portMAX_DELAY) == pdTRUE) {

      // Lock Slave 2's BLE connection
      if (xSemaphoreTake(slave2Mutex, portMAX_DELAY) == pdTRUE) {

        // Check whether Slave 2 is ready
        if (slave2Client != nullptr &&
            slave2Client->isConnected() &&
            slave2Characteristic != nullptr &&
            slave2Characteristic->canWrite()) {

          String command = ledState ? "ON" : "OFF";

          bool success =
              slave2Characteristic->writeValue(command, true);

          Serial.print("Slave 2 Task: ");
          Serial.print(command);
          Serial.println(success ? " sent!" : " failed!");

        } else {
          Serial.println("Slave 2 Task: Slave offline!");
        }

        // Release Slave 2's mutex
        xSemaphoreGive(slave2Mutex);
      }
    }
  }
}

// Called automatically when background BLE scanning finishes
void onReconnectScanComplete(BLEScanResults results) {
  reconnectScanFinished = true;
}

// Start BLE scanning without blocking the main loop
void startReconnectScan() {

  // Prevent multiple scans from starting together
  if (reconnectScanRunning) {
    return;
  }

  reconnectScanFinished = false;

  BLEScan *scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);

  Serial.println("Starting background BLE scan...");

  // Start asynchronous scanning for 5 seconds
  reconnectScanRunning =
      scanner->start(5, onReconnectScanComplete, false);

  if (!reconnectScanRunning) {
    Serial.println("Background BLE scan failed to start!");
  }
}

// Process results after background scanning finishes
void processReconnectScanResults() {

  if (!reconnectScanFinished) {
    return;
  }

  // Mark the scan as completed
  reconnectScanFinished = false;
  reconnectScanRunning = false;

  // Retrieve completed scan results
  BLEScanResults *results =
      BLEDevice::getScan()->getResults();

  Serial.print("Background scan found: ");
  Serial.println(results->getCount());

  // Reset discovery flags
  foundSlave1 = false;
  foundSlave2 = false;

  // Examine discovered BLE devices
  for (int i = 0; i < results->getCount(); i++) {

    BLEAdvertisedDevice device = results->getDevice(i);

    String name = device.getName();
    String address = device.getAddress().toString();

    // Find Slave 1 if reconnection is needed
    if (slave1NeedsReconnect() &&
        (name == SLAVE1_NAME ||
         address == "3c:8a:1f:d3:a7:0a")) {

      slave1Device = device;
      foundSlave1 = true;

      Serial.println("Slave 1 rediscovered!");
    }

    // Find Slave 2 if reconnection is needed
    if (slave2NeedsReconnect() &&
        (name == SLAVE2_NAME ||
         address == "a0:85:e3:e0:15:a1")) {

      slave2Device = device;
      foundSlave2 = true;

      Serial.println("Slave 2 rediscovered!");
    }
  }
}

  void scanForSlaves() {
    // Reset discovery flags before scanning
    foundSlave1 = false;
    foundSlave2 = false;

    // Configure BLE scanner
    BLEScan *scanner = BLEDevice::getScan();
    scanner->setActiveScan(true);

    Serial.println("Scanning for both Slaves...");

    // Scan for 8 seconds
    BLEScanResults *results = scanner->start(8, false);

    Serial.print("Devices found: ");
    Serial.println(results->getCount());

    // Check every discovered device
    for (int i = 0; i < results->getCount(); i++) {

      BLEAdvertisedDevice device = results->getDevice(i);

      String name = device.getName();
      String address = device.getAddress().toString();

      Serial.print("Device Name: ");
      Serial.println(name);

      Serial.print("BLE Address: ");
      Serial.println(address);

      // Identify Slave 1
      if (name == SLAVE1_NAME ||
          address == "3c:8a:1f:d3:a7:0a") {

        slave1Device = device;
        foundSlave1 = true;

        Serial.println("Slave 1 found!");
      }

      // Identify Slave 2
      if (name == SLAVE2_NAME ||
          address == "a0:85:e3:e0:15:a1") {

        slave2Device = device;
        foundSlave2 = true;

        Serial.println("Slave 2 found!");
      }
    }
  }

  // Connect or reconnect to Slave 1
void connectToSlave1Unlocked() {

    if (!foundSlave1) {
      Serial.println("Slave 1 not discovered!");
      return;
    }

    // Skip if Slave 1 is already ready
    if (!slave1NeedsReconnect()) {
      return;
    }

    // Create the client only once
    if (slave1Client == nullptr) {
      slave1Client = BLEDevice::createClient();
    }

    // Connect only when disconnected
    if (!slave1Client->isConnected()) {
      remoteCharacteristic = nullptr;

      Serial.println("Connecting to Slave 1...");

      if (!slave1Client->connect(&slave1Device)) {
        Serial.println("Slave 1 connection failed!");
        return;
      }

      Serial.println("Connected to Slave 1!");
    }

    // Discover the BLE Service
    BLERemoteService *service =
        slave1Client->getService(BLEUUID(SERVICE_UUID));

    if (service == nullptr) {
      Serial.println("Slave 1 Service not found!");
      return;
    }

    // Discover the command Characteristic
    remoteCharacteristic =
        service->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));

    if (remoteCharacteristic != nullptr &&
        remoteCharacteristic->canWrite()) {
      Serial.println("Slave 1 Characteristic ready!");
    } else {
      remoteCharacteristic = nullptr;
      Serial.println("Slave 1 Characteristic not ready!");
    }
  }

  // Safely connect or reconnect to Slave 1
void connectToSlave1() {

  // Try to lock Slave 1's BLE connection
  // Do not wait if the connection is busy
  if (xSemaphoreTake(slave1Mutex, 0) != pdTRUE) {
    Serial.println("Slave 1 BLE busy; retry later.");
    return;
  }

  // Run the existing connection procedure
  connectToSlave1Unlocked();

  // Always release the mutex afterward
  xSemaphoreGive(slave1Mutex);
}

  // Connect or reconnect to Slave 2
void connectToSlave2Unlocked() {

    if (!foundSlave2) {
      Serial.println("Slave 2 not discovered!");
      return;
    }

    // Skip if Slave 2 is already ready
    if (!slave2NeedsReconnect()) {
      return;
    }

    // Create the client only once
    if (slave2Client == nullptr) {
      slave2Client = BLEDevice::createClient();
    }

    // Connect only when disconnected
    if (!slave2Client->isConnected()) {
      slave2Characteristic = nullptr;

      Serial.println("Connecting to Slave 2...");

      if (!slave2Client->connect(&slave2Device)) {
        Serial.println("Slave 2 connection failed!");
        return;
      }

      Serial.println("Connected to Slave 2!");
    }

    // Discover the BLE Service
    BLERemoteService *service =
        slave2Client->getService(BLEUUID(SERVICE_UUID));

    if (service == nullptr) {
      Serial.println("Slave 2 Service not found!");
      return;
    }

    // Discover the command Characteristic
    slave2Characteristic =
        service->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));

    if (slave2Characteristic != nullptr &&
        slave2Characteristic->canWrite()) {
      Serial.println("Slave 2 Characteristic ready!");
    } else {
      slave2Characteristic = nullptr;
      Serial.println("Slave 2 Characteristic not ready!");
    }
  }

// Safely connect or reconnect to Slave 2
void connectToSlave2() {

  // Try to lock Slave 2's BLE connection
  // Do not wait if the connection is busy
  if (xSemaphoreTake(slave2Mutex, 0) != pdTRUE) {
    Serial.println("Slave 2 BLE busy; retry later.");
    return;
  }

  // Run the existing connection procedure
  connectToSlave2Unlocked();

  // Release the mutex afterward
  xSemaphoreGive(slave2Mutex);
}

// Background task for reconnecting missing Slaves
void reconnectWorkerTask(void *parameter) {

  while (true) {

    // Sleep until a reconnection request arrives
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    reconnectWorkerBusy = true;

    // Reconnect Slave 1 when necessary
if (slave1NeedsReconnect() && foundSlave1) {

  connectToSlave1();

  // Synchronize LED only after successful reconnection
  if (!slave1NeedsReconnect()) {

    uint8_t ledState = desiredLedState;

    // Queue the latest button state for Slave 1
    xQueueOverwrite(slave1CommandQueue, &ledState);

    Serial.println("Slave 1 LED state synchronized!");
  }
}

   // Reconnect Slave 2 when necessary
if (slave2NeedsReconnect() && foundSlave2) {

  connectToSlave2();

  // Synchronize LED only after successful reconnection
  if (!slave2NeedsReconnect()) {

    uint8_t ledState = desiredLedState;

    // Queue the latest button state for Slave 2
    xQueueOverwrite(slave2CommandQueue, &ledState);

    Serial.println("Slave 2 LED state synchronization queued!");
  }
}
    reconnectWorkerBusy = false;
  }
}

  void setup() {
    Serial.begin(115200);
    delay(1000);

    // Create independent command queues
slave1CommandQueue = xQueueCreate(1, sizeof(uint8_t));
slave2CommandQueue = xQueueCreate(1, sizeof(uint8_t));

// Check whether queue creation succeeded
if (slave1CommandQueue == nullptr ||
    slave2CommandQueue == nullptr) {

  Serial.println("ERROR: Failed to create command queues!");

  while (true) {
    delay(1000);
  }
}

Serial.println("Both command queues created!");

// Create independent BLE mutexes
slave1Mutex = xSemaphoreCreateMutex();
slave2Mutex = xSemaphoreCreateMutex();

// Verify both mutexes were created
if (slave1Mutex == nullptr || slave2Mutex == nullptr) {
  Serial.println("ERROR: Failed to create BLE mutexes!");

  while (true) {
    delay(1000);
  }
}

Serial.println("Both BLE mutexes created!");

    // Configure button using internal pull-up resistor
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Initialize BLE Master
    BLEDevice::init("LAB7_MASTER");
    Serial.println("Master BLE initialized");

    scanForSlaves();

  // Connect to Slave 1
  connectToSlave1();

  // Connect to Slave 2
  connectToSlave2();

    // Start both LEDs in OFF state
    sendCommandToSlaves("OFF");
    // Start Slave 1 communication task
BaseType_t task1Result = xTaskCreatePinnedToCore(
  slave1CommandTask,
  "Slave1Task",
  8192,
  nullptr,
  1,
  nullptr,
  1
);

// Start Slave 2 communication task
BaseType_t task2Result = xTaskCreatePinnedToCore(
  slave2CommandTask,
  "Slave2Task",
  8192,
  nullptr,
  1,
  nullptr,
  1
);

// Start background reconnection task
BaseType_t reconnectResult = xTaskCreatePinnedToCore(
  reconnectWorkerTask,
  "ReconnectTask",
  8192,
  nullptr,
  1,
  &reconnectTaskHandle,
  1
);

// Check whether all tasks started
if (task1Result != pdPASS ||
    task2Result != pdPASS ||
    reconnectResult != pdPASS) {

  Serial.println("ERROR: Failed to create FreeRTOS tasks!");

  while (true) {
    delay(1000);
  }
}

Serial.println("All FreeRTOS tasks created!");
    Serial.println("Master setup complete!");
  }


  void loop() {

    // Button debounce variables
    static int lastReading = HIGH;
    static int stableState = HIGH;

    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 50;

    // Read the button
    int reading = digitalRead(BUTTON_PIN);

    // Restart debounce timer when reading changes
    if (reading != lastReading) {
      lastDebounceTime = millis();
    }

    // Accept input after 50 ms of stability
    if (millis() - lastDebounceTime > debounceDelay) {

      if (reading != stableState) {

        stableState = reading;

        // Button pressed
        if (stableState == LOW) {

         Serial.println("BUTTON PRESSED");

// LED ON = 1
uint8_t ledState = 1;

desiredLedState = ledState;

// Send the command to each Slave's independent queue
xQueueOverwrite(slave1CommandQueue, &ledState);
xQueueOverwrite(slave2CommandQueue, &ledState);
        }

        // Button released
        else {

          Serial.println("BUTTON RELEASED");

// LED OFF = 0
uint8_t ledState = 0;

desiredLedState = ledState;

// Send the command to each Slave's independent queue
xQueueOverwrite(slave1CommandQueue, &ledState);
xQueueOverwrite(slave2CommandQueue, &ledState);
        }
      }
    }


    lastReading = reading;

  // Process results when background scanning finishes
  if (reconnectScanFinished) {

    processReconnectScanResults();

    // Request background reconnection for discovered Slaves
if (reconnectTaskHandle != nullptr &&
    ((slave1NeedsReconnect() && foundSlave1) ||
     (slave2NeedsReconnect() && foundSlave2))) {

  xTaskNotifyGive(reconnectTaskHandle);

  Serial.println("Background reconnection requested!");
}

    // Start the retry interval after processing
    lastReconnectAttempt = millis();
  }

 if (!reconnectScanRunning &&
    !reconnectWorkerBusy &&
    (slave1NeedsReconnect() || slave2NeedsReconnect())) {

    if (millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {

      lastReconnectAttempt = millis();

      Serial.println("Attempting background BLE reconnection...");

      // Start scanning without blocking loop()
      startReconnectScan();
    }
  }
}