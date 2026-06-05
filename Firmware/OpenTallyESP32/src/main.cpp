#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include "esp_sleep.h"

// ── Pins ──────────────────────────────────────────────────────────────────────
#define PIN_NEOPIXEL   D8
#define PIN_BUTTON     D1
#define PIN_BATT_READ  D0
#define PIN_BATT_ADC   D2
#define NUM_PIXELS     3
#define GPIO_BUTTON    3      // D1 = GPIO3 sur XIAO ESP32C3

// ── Config ────────────────────────────────────────────────────────────────────
#define VBATT_FULL_MV   4200
#define VBATT_EMPTY_MV  3000
#define LONG_PRESS_MS   5000
#define WIFI_TIMEOUT_MS 30000

// ── Globals ───────────────────────────────────────────────────────────────────
Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
WebSocketsClient  wsClient;
WebServer         server(80);
Preferences       prefs;

String   cfg_camera_id;
String   cfg_ws_host;
uint16_t cfg_ws_port;
String   cfg_wifi_ssid;
String   cfg_wifi_pass;

// ── Persistance ───────────────────────────────────────────────────────────────
void loadConfig() {
  prefs.begin("opentally", true);
  cfg_camera_id = prefs.getString("camera_id", "cam01");
  cfg_ws_host   = prefs.getString("ws_host",   "local.orks.fr");
  cfg_ws_port   = prefs.getUShort("ws_port",   20003);
  cfg_wifi_ssid = prefs.getString("wifi_ssid", "MakerSpace - IOT");
  cfg_wifi_pass = prefs.getString("wifi_pass", "makerspace-nuc-07");
  prefs.end();
}

void saveConfig(const String& camera_id, const String& ws_host, uint16_t ws_port,
                const String& wifi_ssid, const String& wifi_pass) {
  prefs.begin("opentally", false);
  prefs.putString("camera_id", camera_id);
  prefs.putString("ws_host",   ws_host);
  prefs.putUShort("ws_port",   ws_port);
  prefs.putString("wifi_ssid", wifi_ssid);
  prefs.putString("wifi_pass", wifi_pass);
  prefs.end();
}

// ── LED helpers ───────────────────────────────────────────────────────────────
void setLeds(uint8_t r, uint8_t g, uint8_t b) {
  pixels.fill(pixels.Color(r, g, b));
  pixels.show();
}

void ledsOff() {
  pixels.clear();
  pixels.show();
}

// ── Batterie ──────────────────────────────────────────────────────────────────
int readBatteryPercent() {
  digitalWrite(PIN_BATT_READ, HIGH);
  delay(10);
  int raw = analogRead(PIN_BATT_ADC);
  digitalWrite(PIN_BATT_READ, LOW);
  int mv = (int)((long)raw * 3300 * 2 / 4095);
  return constrain(map(mv, VBATT_EMPTY_MV, VBATT_FULL_MV, 0, 100), 0, 100);
}

// ── Deep sleep ────────────────────────────────────────────────────────────────
void goDeepSleep() {
  for (int i = 0; i < 3; i++) {
    setLeds(200, 0, 0);
    delay(80);
    ledsOff();
    delay(80);
  }
  delay(100);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

// ── WebSocket ─────────────────────────────────────────────────────────────────
bool wsConnected = false;

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    wsConnected = true;
    Serial.println("WS connecté");
    String sub = "{\"action\":\"SubscribeCamera\",\"camera\":\"" + cfg_camera_id + "\"}";
    String get = "{\"action\":\"GetCameraProperties\",\"camera\":\"" + cfg_camera_id + "\"}";
    wsClient.sendTXT(sub);
    wsClient.sendTXT(get);
    int batt = readBatteryPercent();
    String battMsg = "{\"action\":\"SetBatteryLevel\",\"camera\":\"" + cfg_camera_id
                   + "\",\"content\":{\"batteryLevel\":" + String(batt) + "}}";
    wsClient.sendTXT(battMsg);

  } else if (type == WStype_TEXT) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return;

    bool program = doc["content"]["program"] | false;
    bool preview = doc["content"]["preview"] | false;

    if (program) {
      setLeds(200, 0, 0);      // Rouge  — on air
    } else if (preview) {
      setLeds(0, 200, 0);      // Vert   — preview
    } else {
      ledsOff();               // Idle   — éteint
    }

  } else if (type == WStype_DISCONNECTED) {
    wsConnected = false;
    Serial.println("WS déconnecté");
  }
}

// ── Mode AP + dashboard ───────────────────────────────────────────────────────
void handleConfigPage() {
  int batt = readBatteryPercent();
  bool saved = server.hasArg("saved");

  String banner = saved
    ? "<div class='ok'>Configuration sauvegardée — redémarrage dans <span id='c'>4</span>s..."
      "<script>let n=4;setInterval(()=>{n--;document.getElementById('c').textContent=n},1000)</script>"
      "</div>"
    : "";

  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OpenTally</title><style>"
    "*{box-sizing:border-box}"
    "body{font-family:sans-serif;background:#1a1a2e;color:#eee;"
      "display:flex;justify-content:center;padding:30px 16px;margin:0}"
    ".card{background:#16213e;border-radius:16px;padding:32px;width:100%;max-width:440px}"
    "h1{margin:0 0 4px;font-size:1.4rem}"
    ".batt{color:#aaa;font-size:.9rem;margin-bottom:28px}"
    ".ok{background:#1b4332;border:1px solid #2d6a4f;border-radius:8px;padding:12px 14px;"
      "font-size:.9rem;color:#74c69d;margin-bottom:20px}"
    "label{display:block;font-size:.8rem;color:#aaa;margin:14px 0 4px;text-transform:uppercase;letter-spacing:.05em}"
    "input{width:100%;padding:10px 12px;border-radius:8px;border:1px solid #2a2a4a;"
      "background:#0f3460;color:#fff;font-size:1rem}"
    ".sep{border:none;border-top:1px solid #2a2a4a;margin:20px 0}"
    "button{margin-top:24px;width:100%;padding:13px;border-radius:8px;border:none;"
      "background:#e94560;color:#fff;font-size:1rem;font-weight:700;cursor:pointer}"
    "</style></head><body><div class='card'>"
    "<h1>OpenTally</h1>"
    "<div class='batt'>Batterie : " + String(batt) + " %</div>"
    + banner +
    "<form method='POST' action='/save'>"
    "<label>ID Caméra</label>"
    "<input name='camera_id' value='" + cfg_camera_id + "'>"
    "<hr class='sep'>"
    "<label>Host WebSocket</label>"
    "<input name='ws_host' value='" + cfg_ws_host + "'>"
    "<label>Port WebSocket</label>"
    "<input name='ws_port' type='number' value='" + String(cfg_ws_port) + "'>"
    "<hr class='sep'>"
    "<label>SSID WiFi</label>"
    "<input name='wifi_ssid' value='" + cfg_wifi_ssid + "'>"
    "<label>Mot de passe WiFi</label>"
    "<input name='wifi_pass' type='password' value='" + cfg_wifi_pass + "'>"
    "<button type='submit'>Configurer</button>"
    "</form></div></body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  saveConfig(
    server.arg("camera_id"),
    server.arg("ws_host"),
    (uint16_t)server.arg("ws_port").toInt(),
    server.arg("wifi_ssid"),
    server.arg("wifi_pass")
  );
  loadConfig();  // met à jour les variables globales

  // PRG : redirect GET pour éviter le re-POST au refresh
  server.sendHeader("Location", "/?saved=1");
  server.send(303);

  // Laisse le temps au navigateur de charger la page de confirmation
  delay(4000);
  ESP.restart();
}

void runAPMode() {
  WiFi.softAP("OpenTally-Config", "opentally");
  Serial.print("AP démarré — IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/",      HTTP_GET,  handleConfigPage);
  server.on("/save",  HTTP_POST, handleSave);
  server.begin();

  bool ledState = false;
  unsigned long lastBlink = 0;

  while (true) {
    server.handleClient();
    if (millis() - lastBlink > 800) {
      lastBlink = millis();
      ledState = !ledState;
      if (ledState) setLeds(80, 0, 80); else ledsOff();
    }
  }
}

// ── Mode normal : WiFi + WebSocket ────────────────────────────────────────────
void runNormalMode() {
  WiFi.begin(cfg_wifi_ssid.c_str(), cfg_wifi_pass.c_str());
  Serial.print("Connexion WiFi...");

  bool ledState = false;
  unsigned long lastBlink   = 0;
  unsigned long connectStart = millis();

  // Clignotement bleu pendant la connexion
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStart > WIFI_TIMEOUT_MS) {
      Serial.println("\nTimeout WiFi → deep sleep");
      goDeepSleep();
    }
    if (millis() - lastBlink > 400) {
      lastBlink = millis();
      ledState = !ledState;
      if (ledState) setLeds(0, 0, 200); else ledsOff();
    }
  }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  setCpuFrequencyMhz(80);
  WiFi.setSleep(true);

  // 5 flashs verts à la connexion
  for (int i = 0; i < 5; i++) {
    setLeds(0, 200, 0);
    delay(80);
    ledsOff();
    delay(80);
  }

  // Connexion WebSocket
  wsClient.begin(cfg_ws_host, cfg_ws_port, "/");
  wsClient.onEvent(onWsEvent);
  wsClient.setReconnectInterval(3000);

  // Boucle principale tally
  lastBlink = 0;
  ledState  = false;
  unsigned long lastBattCheck = 0;
  int lastBattPercent = -1;
  unsigned long wsGraceEnd = millis() + 4000;  // 4s de noir avant d'afficher l'erreur WS

  while (true) {
    wsClient.loop();

    // Vérification batterie toutes les 30s, envoi uniquement si le % a changé
    if (wsConnected && millis() - lastBattCheck > 30000) {
      lastBattCheck = millis();
      int batt = readBatteryPercent();
      if (batt != lastBattPercent) {
        lastBattPercent = batt;
        String msg = "{\"action\":\"SetBatteryLevel\",\"camera\":\"" + cfg_camera_id
                   + "\",\"content\":{\"batteryLevel\":" + String(batt) + "}}";
        wsClient.sendTXT(msg);
        Serial.println("Batterie envoyée : " + String(batt) + "%");
      }
    }

    // Blink rouge lent si WS déconnecté (après la période de grâce)
    if (!wsConnected && millis() > wsGraceEnd) {
      if (millis() - lastBlink > 900) {
        lastBlink = millis();
        ledState = !ledState;
        if (ledState) setLeds(180, 0, 0); else ledsOff();
      }
    }

    // Appui bouton → deep sleep
    if (digitalRead(PIN_BUTTON) == LOW) {
      delay(50);
      if (digitalRead(PIN_BUTTON) == LOW) {
        while (digitalRead(PIN_BUTTON) == LOW) delay(10);
        goDeepSleep();
      }
    }
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BATT_READ, OUTPUT);
  digitalWrite(PIN_BATT_READ, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  pixels.begin();
  pixels.setBrightness(80);
  ledsOff();

  loadConfig();

  // Mesure durée d'appui pour déterminer le mode
  unsigned long pressStart = millis();
  bool longPress = false;

  while (digitalRead(PIN_BUTTON) == LOW) {
    if (millis() - pressStart >= LONG_PRESS_MS) {
      longPress = true;
      setLeds(80, 0, 80);   // Violet = long press confirmé
      break;
    }
    delay(10);
  }

  // Attendre relâchement
  while (digitalRead(PIN_BUTTON) == LOW) delay(10);

  if (longPress) {
    runAPMode();
  } else {
    runNormalMode();
  }
}

void loop() {}
