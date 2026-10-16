# BCA188 Laboratory Activity 7 — Modular ESP32 BLE Master

This is the **Master** firmware for the previously tested ESP32 BLE setup. It is a modularized FreeRTOS version, based on the Step 100 sketch. The **Slave 1** and **Slave 2** sketches are unchanged.

## Board and wiring

- 🟡 Yellow ESP32-WROOM-32: **ESP32 Dev Module**, Arduino ESP32 Core **3.3.11**.
- Push button connects **GPIO 23 to GND**. `INPUT_PULLUP` is enabled.
- ⚫ Black ESP32-WROOM-32 runs `LAB7_SLAVE_1` (LED on GPIO 18).
- 🔵 ESP32-S3 runs `LAB7_SLAVE_2` (LED on GPIO 18).
- Both Slaves have the same BLE Service/Characteristic UUIDs defined in `Config.h`.
- **No phone, Wi-Fi, or third-party BLE library is necessary** for this version.

## Open it in Arduino IDE

1. Extract the ZIP archive and **keep all ten files in the same `master_modular` folder**.
2. Double-click **`master_modular.ino`**. Arduino should show tabs for `.h` / `.cpp` files.
3. Select **ESP32 Dev Module**, choose the Master's COM port, and set Upload Speed to 115200.
4. Click **Verify** first. Then upload to the **Yellow Master only**.
5. Open Serial Monitor at 115200 baud. Power both Slaves before starting the Master.

### Arduino folder rule

The sketch filename `master_modular.ino` matches its containing folder `master_modular/`. If you rename the folder, also rename the `.ino` file to match.

## What each file does

| File | Purpose |
|---|---|
| `master_modular.ino` | Short `setup()`/`loop()` that organizes initialization and polling |
| `Config.h` | Names, GPIO pin, UUIDs, scan and retry timings |
| `SharedState.h` | Shared variable declarations (`extern`) |
| `SharedState.cpp` | Exactly one definition of each shared variable |
| `BLEManager.h` | Public BLE function declarations |
| `BLEManager.cpp` | Scan, identify and reconnect the BLE Slaves |
| `RTOSTasks.h` | FreeRTOS task/queue function declarations |
| `RTOSTasks.cpp` | Two independent command tasks + background reconnect task |
| `ButtonHandler.h` | Button interface declarations |
| `ButtonHandler.cpp` | `INPUT_PULLUP`, 50-ms debounce, queue commands |

## Expected behavior

- **Press** the button: both connected Slave LEDs turn ON.
- **Release** the button: both turn OFF.
- Disconnect either Slave: the surviving Slave should keep receiving commands.
- Restore the missing Slave: the Master should find it again and queue the latest desired LED state.
- Retries have a 5-second interval plus scanning/connecting time.

### Important testing limitations

This modular version is a **new refactoring** of the hardware-tested Step 100 sketch. **It has not yet been compiled or physically tested on your three boards.** Source review is not a substitute for real testing. FreeRTOS improves separation, but BLE-radio contention and timeouts can still cause short latency.

Keep your previous tested RTOS and pre-RTOS master sketches as backups. Test normal control first, then power-loss recovery one Slave at a time, then holding the button while reconnecting.

## Official BLE library

Built into **esp32 by Espressif Systems**, not separately installed:
https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE
