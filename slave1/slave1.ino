
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// Shared BLE UUIDs
#define SERVICE_UUID "ec8817e4-164d-4c5e-8f2f-9b238a1b4e23"
#define CHARACTERISTIC_UUID "e70df770-9853-4704-bd6d-d18b3f8dfdb3"

// LED connected to GPIO 18
#define LED_PIN 18

// Handle commands received from the Master
class CommandCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *characteristic) override {

    auto command = characteristic->getValue();

    Serial.print("Received command: ");
    Serial.println(command.c_str());

    // Turn LED 1 ON
    if (command == "ON") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED 1 ON");
    }

    // Turn LED 1 OFF
    else if (command == "OFF") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED 1 OFF");
    }
  }
};

// Handle BLE connection and disconnection
class ServerCallbacks : public BLEServerCallbacks {
public:
  void onConnect(BLEServer *server) override {
    Serial.println("Slave 1 connected to Master!");
  }

  void onDisconnect(BLEServer *server) override {
    Serial.println("Slave 1 disconnected!");

    // Restart advertising so the Master can reconnect
    BLEDevice::startAdvertising();

    Serial.println("Slave 1 advertising restarted!");
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure LED 1
  pinMode(LED_PIN, OUTPUT);

  // Keep LED OFF at startup
  digitalWrite(LED_PIN, LOW);

  // Initialize BLE Slave 1
  BLEDevice::init("LAB7_SLAVE_1");

  // Create BLE Server
  BLEServer *server = BLEDevice::createServer();

  // Assign connection event handler
  server->setCallbacks(new ServerCallbacks());

  // Create BLE Service
  BLEService *service = server->createService(SERVICE_UUID);
  
  // Create writable Characteristic
  BLECharacteristic *characteristic =
      service->createCharacteristic(
          CHARACTERISTIC_UUID,
          BLECharacteristic::PROPERTY_WRITE
      );

  // Assign command handler
  characteristic->setCallbacks(new CommandCallbacks());

  // Start BLE Service
  service->start();

  // Configure BLE advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();

  // Advertise the shared Service UUID
  advertising->addServiceUUID(SERVICE_UUID);

  // Start advertising
  advertising->start();

  Serial.println("Slave 1 ready; advertising BLE");
}

void loop() {
  // BLE commands are handled automatically by callbacks
}
