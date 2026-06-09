# OpenTallyESP32

A wireless tally light system built around the **Seeed XIAO ESP32-C3**. Each unit connects to a WebSocket tally server over Wi-Fi and lights up its two NeoPixel LEDs to show the live camera state (Program / Preview / Idle). Battery level is reported to the server automatically.

This project was created for the [orKs](https://lce.gg/) association for the **LCE** ([lce.gg](https://lce.gg/)) event, to manage camera tally signaling during live productions.

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | Seeed Studio XIAO ESP32-C3 |
| LEDs | 3× NeoPixel (WS2812B), pin **D8** — 2 rear + 1 front-facing |
| Button | Tactile switch, pin **D1** (GPIO 3) |
| Battery sensing | Enable pin **D0**, ADC pin **D2** — voltage divider, 3.0 V–4.2 V range |

The KiCad schematic and PCB layout are in [`ECAD/OpenTallyESP32/`](ECAD/OpenTallyESP32/).

---

## Firmware

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)

### Credentials

Sensitive values (WiFi credentials, WebSocket server) are kept out of version control in a local file:

```
Firmware/OpenTallyESP32/include/credentials.h
```

This file is listed in `.gitignore` and will never be committed. Before building, copy the example file and fill in your values:

```bash
cp include/credentials.h.example include/credentials.h
```

```cpp
#define DEFAULT_WIFI_SSID  "your_ssid"
#define DEFAULT_WIFI_PASS  "your_password"
#define DEFAULT_WS_HOST    "your.websocket.server"
#define DEFAULT_WS_PORT    80
```

These defines are used as fallback defaults when no configuration has been saved to flash yet. Once configured via the dashboard, the flash values take over and these defaults are no longer used.

### Dependencies (resolved automatically by PlatformIO)

- `adafruit/Adafruit NeoPixel`
- `links2004/WebSockets`
- `bblanchon/ArduinoJson`

---

## Configuration

### NVS flash storage

Configuration is persisted in the ESP32's **NVS (Non-Volatile Storage)** — a key-value store built into the flash that survives resets and power cuts. The Arduino `Preferences` library is used to read and write it under the namespace `opentally`.

Values are written only when the user submits the config form; they are read once at every boot before anything else runs. If a key has never been written, the firmware falls back to the compiled-in default shown below.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `camera_id` | string | `cam01` | Camera identifier sent in every WebSocket message |
| `ws_host` | string | `your.websocket.server` | WebSocket server hostname |
| `ws_port` | uint16 | `80` | WebSocket server port |
| `wifi_ssid` | string | — | Wi-Fi network name |
| `wifi_pass` | string | — | Wi-Fi password |

### Entering configuration mode

Hold the button for **5 seconds** at power-on (or after waking from deep sleep). The LEDs will turn **solid purple** to confirm the long-press was detected.

The device starts a Wi-Fi access point:

- **SSID:** `OpenTally-Config`
- **Password:** `opentally`

Connect to it and open **http://192.168.4.1** in a browser. Fill in the form and click *Configurer*. The device saves the settings to NVS, confirms with a countdown banner, then restarts into normal mode.

### Operating modes — flowchart

```
                        ┌─────────────────┐
                        │   Power on /    │
                        │  Wake from deep │
                        │      sleep      │
                        └────────┬────────┘
                                 │
                        Load NVS config
                                 │
                    ┌────────────▼────────────┐
                    │  Button held on boot?   │
                    └────┬──────────────┬─────┘
                  Yes ≥5s│              │No
                         │              │
           ┌─────────────▼──┐    ┌──────▼───────────────┐
           │   AP / Config  │    │     Normal mode      │
           │     mode       │    │  Connect to Wi-Fi    │
           │ SSID: OpenTally│    └─────┬────────────────┘
           │ IP: 192.168.4.1│          │
           │ LEDs: purple   │    ┌─────▼─────────────────────┐
           │   blink        │    │  Wi-Fi connected in 30s?  │
           └─────────┬──────┘    └──┬─────────────────────┬──┘
                     │           No │                     │ Yes
              Serve config    ┌─────▼──────┐    ┌─────────▼──────────┐
                  form        │ Deep sleep │    │  Connect WebSocket │
                     │        └────────────┘    └─────────┬──────────┘
              User submits                                │
                     │                       ┌────────────▼───────────┐
              Save to NVS                    │      Main loop         │
                     │                       │  ┌──────────────────┐  │
                  Restart                    │  │ WS msg received  │  │
                                             │  │  → update LEDs   │  │
                                             │  │    RED / GREEN / │  │
                                             │  │      OFF         │  │
                                             │  └──────────────────┘  │
                                             │  ┌──────────────────┐  │
                                             │  │ Every 30s        │  │
                                             │  │ → send battery % │  │
                                             │  └──────────────────┘  │
                                             │  ┌──────────────────┐  │
                                             │  │ Button press     │  │
                                             │  │  → deep sleep    │  │
                                             │  └──────────────────┘  │
                                             └────────────────────────┘
```

---

## Operation

### Boot sequence

1. Device loads saved configuration from NVS.
2. If button is held ≥ 5 s → **AP / config mode** (see above).
3. Otherwise → **normal mode**: connects to the configured Wi-Fi.

### LED states

The device has two **rear LEDs** (visible to the operator) and one **front LED** (visible to the talent, facing the subject).

**Tally states**

| State | Rear LEDs (0 & 1) | Front LED (2) |
|-------|-------------------|---------------|
| Program (on air) | Red | Red |
| Preview | Green | Off |
| Program + Preview | Red (program takes priority) | Red |
| Idle | Off | Off |

**Status patterns**

Slow-blink patterns affect rear LEDs only (front LED stays off). Quick-flash patterns fill all 3 LEDs.

| Color | Pattern | Front LED | Meaning |
|-------|---------|-----------|---------|
| Blue | Slow blink | Off | Connecting to Wi-Fi |
| Green | 5 quick flashes | On | Wi-Fi connected |
| Red | Slow blink | Off | WebSocket disconnected |
| Purple | Slow blink | Off | AP config mode active |
| Red | 3 quick flashes | On | Entering deep sleep |

### Power saving

- After Wi-Fi connects the CPU is throttled to **80 MHz** and Wi-Fi modem-sleep is enabled.
- Press the button at any time — including during Wi-Fi connection — to immediately enter **deep sleep**. The device will wake again on the next button press.
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

[`tally-control.html`](tally-control.html) is a standalone HTML file intended primarily for **testing purposes** — useful to verify the WebSocket server is working and to trigger tally states without needing physical hardware. Open it in any browser, enter the WebSocket URL (e.g. `ws://your.websocket.server:80/`) and the camera ID, then click **Connecter**. You can then manually set a camera to **Idle**, **Preview**, or **Program**. All messages sent and received are shown in the log panel.

---

## Repository structure

```
OpenTallyESP32/
├── ECAD/
│   ├── LIB/                  KiCad symbol and footprint library (XIAO ESP32-C3)
│   └── OpenTallyESP32/       KiCad project (schematic, PCB layout)
│       ├── *.kicad_*         KiCad project files
│       ├── *.gbr / *.gbrjob  Gerber files for PCB manufacturing
│       ├── *.dxf             DXF exports
│       └── OpenTallyESP32PCB.step  3D STEP model
├── Firmware/
│   └── OpenTallyESP32/
│       ├── src/main.cpp      Full firmware source
│       ├── include/          credentials.h.example (copy to credentials.h before building)
│       ├── test/             Manual test sketches
│       └── platformio.ini    PlatformIO project config
└── tally-control.html        Browser-based tally controller
```
