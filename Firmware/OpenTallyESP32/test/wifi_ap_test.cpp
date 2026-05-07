// Test: Access Point simple
// Flasher ce fichier manuellement en le copiant dans src/main.cpp

#include <Arduino.h>
#include <WiFi.h>

const char* AP_SSID = "OpenTally-Test";
const char* AP_PASSWORD = "12345678";

void setup() {
  Serial.begin(115200);

  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println("Access Point démarré");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  Serial.print("Clients connectés: ");
  Serial.println(WiFi.softAPgetStationNum());
  delay(5000);
}
