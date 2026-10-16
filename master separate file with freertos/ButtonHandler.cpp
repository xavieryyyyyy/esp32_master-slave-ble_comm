#include <Arduino.h>
#include "ButtonHandler.h"
#include "Config.h"
#include "SharedState.h"
#include "RTOSTasks.h"

namespace ButtonHandler {

// These variables belong ONLY to this module, not the whole project.
static int lastReading = HIGH;
static int stableState = HIGH;
static unsigned long lastChangeMs = 0;

void begin() {
  // With INPUT_PULLUP, the pin reads HIGH when released and LOW
  // when a button connects GPIO 23 to GND (pressed).
  pinMode(Config::BUTTON_PIN, INPUT_PULLUP);

  lastReading = digitalRead(Config::BUTTON_PIN);
  stableState = lastReading;
  State::desiredLedState.store(stableState == LOW ? 1 : 0);
  Serial.println("Master button initialized");
}

void update() {
  int reading = digitalRead(Config::BUTTON_PIN);

  // Restart our 50-ms debounce timer when the contact bounces.
  if (reading != lastReading) {
    lastChangeMs = millis();
  }

  // A state change is real only after the signal stays stable.
  if (millis() - lastChangeMs >= Config::DEBOUNCE_MS &&
      reading != stableState) {

    stableState = reading;
    uint8_t ledState = stableState == LOW ? 1 : 0;
    State::desiredLedState.store(ledState);

    Serial.println(ledState ? "BUTTON PRESSED" : "BUTTON RELEASED");

    // This operation is quick. The FreeRTOS workers handle BLE writes.
    RTOSTasks::queueForBoth(ledState);
  }

  lastReading = reading;
}

} // namespace ButtonHandler
