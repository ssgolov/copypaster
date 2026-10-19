# copypaster

# ESP32 Music Remote

**A customizable Bluetooth media remote and macro keypad powered by ESP32.**

ESP32 Music Remote is a programmable four-button controller that lets you control media playback, adjust volume, and execute custom keyboard shortcuts on your computer over Bluetooth Low Energy (BLE).

The device features configurable short and long presses, five user-defined profiles, and a built-in web interface for configuration — no dedicated desktop application required.

## Features

- **Bluetooth Low Energy** — connects to your computer as a BLE keyboard.
- **4 programmable buttons** — configure each button independently.
- **Short and long press actions** — assign different actions depending on how long a button is held.
- **Media controls** — volume up/down, mute, play/pause, next track, previous track, and stop.
- **Custom keyboard shortcuts** — combine modifier keys with a selected keyboard key.
- **Five configuration profiles** — save, load, and rename profiles.
- **Windows and macOS support** — select the target operating system in the configuration interface.
- **Web-based configuration** — configure buttons and profiles using a browser.
- **Persistent settings** — configuration is stored in ESP32 non-volatile memory.
- **Automatic Wi-Fi shutdown** — the configuration access point is disabled after three minutes.

## Hardware

- ESP32 development board
- 4 momentary push buttons
- Connecting wires
- USB power supply or another suitable power source

The exact hardware requirements depend on the ESP32 board and the physical implementation.

## Default Button Mapping

| Button | Short Press | Long Press |
|---|---|---|
| Volume Down | Volume Down | Mute |
| Previous | Previous Track | Previous Track |
| Next | Next Track | Next Track |
| Volume Up | Volume Up | Play/Pause |

All actions can be customized through the web interface.

## Pin Configuration

| Button | GPIO |
|---|---:|
| Volume Down | 8 |
| Previous Track | 7 |
| Next Track | 6 |
| Volume Up | 5 |

The firmware uses the ESP32's internal pull-up resistors, so buttons should be wired between their assigned GPIO pins and GND.

**Note:** Check GPIO availability and restrictions for your specific ESP32 board before building the circuit.

## Software Requirements

- Arduino IDE or a compatible Arduino build environment
- ESP32 board support package
- `WiFi.h`
- `WebServer.h`
- `Preferences.h`
- `esp_wifi.h`
- `HijelHID_BLEKeyboard` library

Install the ESP32 board package and the required BLE keyboard library before compiling the firmware.

## Getting Started

1. Connect the four push buttons to the GPIO pins listed above and GND.
2. Open the firmware sketch in Arduino IDE.
3. Select the appropriate ESP32 board and port.
4. Compile and upload the firmware.
5. Enable Bluetooth on your computer.
6. Pair with **ESP32 Music Remote**.

Once connected, the device sends media commands and keyboard shortcuts according to the active configuration.

## Web Configuration

The firmware includes a browser-based configuration interface hosted directly on the ESP32.

To enter configuration mode:

1. Disconnect or reset the device.
2. Hold the **Volume Down** button while the ESP32 starts.
3. Connect to the Wi-Fi access point:
   - **SSID:** `ESP32-Music-Remote`
   - **Password:** `12345678`
4. Open the access point's gateway address in a browser. The firmware prints the IP address to the serial monitor.
5. Configure button actions, select the target operating system, and manage profiles.
6. Save the configuration.

The configuration access point automatically shuts down after three minutes.

**Security note:** Change the default access point password before using the device in an untrusted environment. The configuration interface uses HTTP and should be treated as a local, unencrypted interface.

## Custom Actions

Each button supports two independently configurable actions:

- **Short press**
- **Long press** — triggered after approximately 700 ms.

Available action types:

- No action
- Media command
- Keyboard shortcut

Keyboard shortcuts support modifier keys such as Ctrl, Shift, Alt, and the system key. Modifier labels are adapted for Windows and macOS.

## Profiles

The firmware supports five persistent profiles. Each profile stores:

- Target operating system
- Short-press action for each button
- Long-press action for each button
- Custom profile name

Profiles can be loaded, saved, and renamed through the web interface.

## Project Structure

The project is implemented in a single Arduino sketch containing:

- BLE keyboard initialization and command execution
- Button state and press-duration detection
- Media and keyboard shortcut handling
- Persistent configuration and profile management
- Embedded HTML/CSS/JavaScript configuration interface
- Wi-Fi access point lifecycle management

## Limitations

- The firmware supports four physical buttons in its current implementation.
- The configuration interface is available only while configuration mode is active.
- Button actions are processed when the BLE keyboard is connected.
- Wi-Fi configuration mode uses a predefined access point name and password.
- Actual compatibility depends on the ESP32 board, BLE stack, and host operating system.
