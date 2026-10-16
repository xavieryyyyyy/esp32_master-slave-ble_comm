#pragma once

// =============================================================
// Button wiring and debouncing, separated from the BLE code.
// =============================================================
namespace ButtonHandler {

void begin();  // Configure GPIO 23 as INPUT_PULLUP.
void update(); // Poll the button quickly from Arduino loop().

} // namespace ButtonHandler
