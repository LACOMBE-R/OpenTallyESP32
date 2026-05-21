# OpenTallyESP32

A wireless tally light system built around the **Seeed XIAO ESP32-C3**. Each unit connects to a WebSocket tally server over Wi-Fi and lights up its two NeoPixel LEDs to show the live camera state (Program / Preview / Idle). Battery level is reported to the server automatically.

This project was created for the [orKs](https://lce.gg/) association for the **LCE** ([lce.gg](https://lce.gg/)) event, to manage camera tally signaling during live productions.

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | Seeed Studio XIAO ESP32-C3 |
| LEDs | 2× NeoPixel (WS2812B), pin **D8** |
| Button | Tactile switch, pin **D1** (GPIO 3) |
| Battery sensing | Enable pin **D0**, ADC pin **D2** — voltage divider, 3.0 V–4.2 V range |

The KiCad schematic and PCB layout are in [`ECAD/OpenTallyESP32/`](ECAD/OpenTallyESP32/).

---

## Firmware

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB-C cable to flash the XIAO

### Dependencies (resolved automatically by PlatformIO)

- `adafruit/Adafruit NeoPixel`
- `links2004/WebSockets`
- `bblanchon/ArduinoJson`

### Build & Flash

```bash
# from the firmware directory
cd Firmware/OpenTallyESP32

# build
pio run

# flash (auto-detects port)
pio run --target upload

# open serial monitor (115200 baud)
pio device monitor
```

---

## Configuration

Configuration is stored in the ESP32 NVS flash under the namespace `opentally`. The following parameters are saved:

| Key | Default | Description |
|-----|---------|-------------|
| `camera_id` | `cam01` | Camera identifier used when subscribing to the tally server |
| `ws_host` | `local.orks.fr` | WebSocket server hostname |
| `ws_port` | `20003` | WebSocket server port |
| `wifi_ssid` | — | Wi-Fi network name |
| `wifi_pass` | — | Wi-Fi password |

### Entering configuration mode

Hold the button for **5 seconds** at power-on (or after waking from deep sleep). The LEDs will turn **solid purple** to confirm the long-press was detected.

The device starts a Wi-Fi access point:

- **SSID:** `OpenTally-Config`
- **Password:** `opentally`

Connect to it and open **http://192.168.4.1** in a browser. Fill in the form and click *Configurer*. The device saves the settings, confirms with a countdown banner, then restarts into normal mode.

---

## Operation

### Boot sequence

1. Device loads saved configuration from NVS.
2. If button is held ≥ 5 s → **AP / config mode** (see above).
3. Otherwise → **normal mode**: connects to the configured Wi-Fi.

### LED states

| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Slow blink | Connecting to Wi-Fi |
| Green | 5 quick flashes | Wi-Fi connected |
| Red | Solid | Camera is **Program** (on air) |
| Green | Solid | Camera is **Preview** |
| Off | — | Camera is **Idle** |
| Red | Slow blink | Wi-Fi connected but WebSocket disconnected |
| Purple | Slow blink | AP config mode active |
| Red | 3 quick flashes | Entering deep sleep |

### Power saving

- After Wi-Fi connects the CPU is throttled to **80 MHz** and Wi-Fi modem-sleep is enabled.
- Press the button at any time during normal operation to enter **deep sleep**. The device will wake again on the next button press.
- If Wi-Fi does not connect within **30 seconds**, the device goes to deep sleep automatically.

### Battery reporting

On WebSocket connection, and every **30 seconds** thereafter, the device reads the battery voltage and sends the percentage to the tally server (only if the value changed). The server can display it in any connected control surface.

---

## WebSocket protocol

> **Note:** The WebSocket backend used at LCE is not open source. It pulls live camera states directly from a **Blackmagic ATEM** video switcher and broadcasts them to all connected tally units. However, the protocol is straightforward enough to reimplement — any server that speaks the JSON messages below can drive these tally units.

All messages are JSON objects with at least an `action` field and, where relevant, a `camera` field and a `content` object.

### Messages sent by the tally unit (firmware)

| Action | Extra fields | Description |
|--------|-------------|-------------|
| `SubscribeCamera` | `camera` | Subscribe to state updates for the given camera ID. The server will push updates whenever the camera's program/preview state changes. |
| `GetCameraProperties` | `camera` | Request the current program/preview state of a camera immediately after connecting. |
| `SetBatteryLevel` | `camera`, `content.batteryLevel` (integer 0–100) | Report the tally unit's battery percentage to the server. Sent on connection, then every 30 s if the value changed. |

### Messages sent by the browser controller (`tally-control.html`)

| Action | Extra fields | Description |
|--------|-------------|-------------|
| `ActAsSender` | — | Identifies this client as a control surface (sender), not a passive tally receiver. |
| `SubscribeCamera` | `camera` | Subscribe to state updates for a camera (same as firmware). |
| `GetCameraProperties` | `camera` | Request the current state immediately. |
| `SetCameraProperties` | `camera`, `content.program` (bool), `content.preview` (bool) | Set the tally state of a camera. Both fields must be present; at most one should be `true`. |

### Messages received from the server

| Trigger | Expected fields | Description |
|---------|----------------|-------------|
| State change or reply to `GetCameraProperties` | `content.program` (bool), `content.preview` (bool) | Pushed to all subscribers whenever a camera's state changes. The firmware and browser controller both read these two booleans to update their display. |

---

## Browser control tool

[`tally-control.html`](tally-control.html) is a standalone HTML file intended primarily for **testing purposes** — useful to verify the WebSocket server is working and to trigger tally states without needing physical hardware. Open it in any browser, enter the WebSocket URL (e.g. `ws://local.orks.fr:20003/`) and the camera ID, then click **Connecter**. You can then manually set a camera to **Idle**, **Preview**, or **Program**. All messages sent and received are shown in the log panel.

---

## Repository structure

```
OpenTallyESP32/
├── ECAD/
│   ├── LIB/                  KiCad symbol and footprint library (XIAO ESP32-C3)
│   └── OpenTallyESP32/       KiCad schematic and PCB layout
├── Firmware/
│   └── OpenTallyESP32/
│       ├── src/main.cpp      Full firmware source
│       ├── test/             Manual test sketches
│       └── platformio.ini    PlatformIO project config
└── tally-control.html        Browser-based tally controller
```
