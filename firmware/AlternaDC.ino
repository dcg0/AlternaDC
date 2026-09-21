/* AlternaDC — DC Laboratory
 * Dual INA226 monitor, local AP dashboard, automatic lights and piezo alerts.
 * Original project attribution: tipih/12VBatteryMonitor (see LICENSE).
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace Config {
constexpr uint8_t INA_MAIN = 0x40;
constexpr uint8_t INA_SECONDARY = 0x41;
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr int RELAY_PIN = 26;
constexpr int BUZZER_PIN = 27;
constexpr float SHUNT_OHMS = 0.001f;
constexpr float CAPACITY_AH = 100.0f;
constexpr uint32_t SAMPLE_MS = 1000;
constexpr uint32_t LOW_ALERT_MS = 30000;
}

WebServer server(80);
struct Channel {
  const char* id; const char* label; uint8_t address; bool online;
  float voltage, current, power, energyWh, chargePct; uint32_t lastRead;
};
Channel mainChannel{"main", "Alternador Principal", Config::INA_MAIN, false, 0, 0, 0, 0, 0, 0};
Channel secondaryChannel{"secondary", "Alternador Secundario", Config::INA_SECONDARY, false, 0, 0, 0, 0, 0, 0};

enum LightMode { LIGHT_AUTO, LIGHT_ON, LIGHT_OFF };
LightMode lightMode = LIGHT_AUTO;
bool lightsOn = false;
bool muted = false;
uint32_t lastSample = 0, lastLowAlert = 0;
String alertMessage = "Sistema iniciado";

bool readRegister16(uint8_t address, uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(address); Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)address, 2) != 2) return false;
  value = (uint16_t(Wire.read()) << 8) | Wire.read(); return true;
}
void writeRegister16(uint8_t address, uint8_t reg, uint16_t value) {
  Wire.beginTransmission(address); Wire.write(reg); Wire.write(value >> 8); Wire.write(value & 0xff); Wire.endTransmission();
}
bool initINA226(Channel &ch) {
  uint16_t id = 0; if (!readRegister16(ch.address, 0xFE, id)) { ch.online = false; return false; }
  // 16 averages, 1.1 ms bus/shunt conversion, continuous shunt+bus.
  writeRegister16(ch.address, 0x00, 0x4527);
  // Current LSB = 0.1 mA; calibration = 0.00512/(0.0001*Rshunt).
  writeRegister16(ch.address, 0x05, uint16_t(0.00512f / (0.0001f * Config::SHUNT_OHMS)));
  ch.online = true; return true;
}
bool readChannel(Channel &ch) {
  uint16_t busRaw = 0, shuntRaw = 0;
  if (!readRegister16(ch.address, 0x02, busRaw) || !readRegister16(ch.address, 0x01, shuntRaw)) { ch.online = false; return false; }
  ch.online = true;
  ch.voltage = busRaw * 0.00125f;
  int16_t signedShunt = int16_t(shuntRaw);
  ch.current = (signedShunt * 0.0000025f) / Config::SHUNT_OHMS;
  ch.power = ch.voltage * ch.current;
  const float hours = (millis() - ch.lastRead) / 3600000.0f;
  if (ch.lastRead && isfinite(ch.power)) ch.energyWh += max(0.0f, ch.power) * hours;
  ch.lastRead = millis();
  ch.chargePct = constrain((ch.voltage - 11.8f) * 100.0f / (14.2f - 11.8f), 0.0f, 100.0f);
  return true;
}
void beep(uint16_t frequency, uint16_t duration) { if (!muted) { ledcWriteTone(0, frequency); delay(duration); ledcWriteTone(0, 0); } }
void bootChime() { beep(880, 100); delay(70); beep(1320, 150); }
void updateLightsAndAlerts() {
  const float v = max(mainChannel.voltage, secondaryChannel.voltage);
  if (!mainChannel.online || !secondaryChannel.online) alertMessage = "Fallo independiente en un canal";
  else if (v < 12.7f) alertMessage = "Voltaje bajo: luces extras apagadas";
  else if (v < 13.8f) alertMessage = "Carga media: solo luces esenciales";
  else alertMessage = "Carga plena: luces extras disponibles";
  bool automatic = v > 13.8f;
  lightsOn = lightMode == LIGHT_ON || (lightMode == LIGHT_AUTO && automatic);
  digitalWrite(Config::RELAY_PIN, lightsOn ? HIGH : LOW);
  if (v > 13.8f && automatic && millis() - lastLowAlert > Config::LOW_ALERT_MS) { beep(1760, 100); delay(80); beep(1760, 100); lastLowAlert = millis(); }
  if (v < 12.7f && millis() - lastLowAlert > Config::LOW_ALERT_MS) { beep(440, 250); lastLowAlert = millis(); }
}
String lightModeName() { return lightMode == LIGHT_AUTO ? "auto" : lightMode == LIGHT_ON ? "on" : "off"; }
void sendFile(const char* path, const char* type) { if (!LittleFS.exists(path)) { server.send(404, "text/plain", "Archivo no encontrado"); return; } File f = LittleFS.open(path, "r"); server.streamFile(f, type); f.close(); }
void handleData() {
  JsonDocument doc; doc["brand"] = "AlternaDC"; doc["uptime_ms"] = millis(); doc["lights"]["on"] = lightsOn; doc["lights"]["mode"] = lightModeName(); doc["muted"] = muted; doc["alert"] = alertMessage;
  for (Channel* ch : {&mainChannel, &secondaryChannel}) { JsonObject o = doc["channels"][ch->id].to<JsonObject>(); o["label"] = ch->label; o["address"] = String("0x") + String(ch->address, HEX); o["online"] = ch->online; o["voltage"] = ch->voltage; o["current"] = ch->current; o["power"] = ch->power; o["energyWh"] = ch->energyWh; o["chargePct"] = ch->chargePct; }
  String out; serializeJson(doc, out); server.send(200, "application/json", out);
}
void handleLight() { if (!server.hasArg("mode")) { server.send(400, "text/plain", "mode requerido"); return; } String m = server.arg("mode"); if (m == "auto") lightMode = LIGHT_AUTO; else if (m == "on") lightMode = LIGHT_ON; else lightMode = LIGHT_OFF; updateLightsAndAlerts(); server.send(200, "application/json", "{\"ok\":true}"); }
void setupServer() {
  server.on("/", HTTP_GET, [](){ sendFile("/index.html", "text/html; charset=utf-8"); });
  server.on("/style.css", HTTP_GET, [](){ sendFile("/style.css", "text/css"); });
  server.on("/app.js", HTTP_GET, [](){ sendFile("/app.js", "application/javascript"); });
  server.on("/api/data", HTTP_GET, handleData); server.on("/api/lights", HTTP_POST, handleLight);
  server.on("/api/mute", HTTP_POST, [](){ muted = !muted; server.send(200, "application/json", String("{\"muted\":") + (muted ? "true}" : "false}")); });
  server.onNotFound([](){ server.send(404, "text/plain", "No encontrado"); }); server.begin();
}
void setup() {
  Serial.begin(115200); pinMode(Config::RELAY_PIN, OUTPUT); digitalWrite(Config::RELAY_PIN, LOW); ledcSetup(0, 2000, 8); ledcAttachPin(Config::BUZZER_PIN, 0);
  Wire.begin(Config::SDA_PIN, Config::SCL_PIN); initINA226(mainChannel); initINA226(secondaryChannel);
  if (!LittleFS.begin(true)) Serial.println("LittleFS no disponible");
  WiFi.mode(WIFI_AP); WiFi.softAP("AlternaDC", "alternadc"); setupServer(); bootChime();
  Serial.println("AlternaDC activo en http://192.168.4.1");
}
void loop() { server.handleClient(); if (millis() - lastSample >= Config::SAMPLE_MS) { lastSample = millis(); readChannel(mainChannel); readChannel(secondaryChannel); updateLightsAndAlerts(); } }

// Fin del firmware AlternaDC.
