#include "mqtt_handler.h"
#include "config.h"
#include "sensors.h"
#include "safety.h"
#include "button.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// -------------------- MQTT CLIENT --------------------
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// -------------------- INTERNAL -----------------------
static unsigned long lastPublish = 0;
static const unsigned long publishIntervalMs = 2000; // 2 seconds

// -------------------- INIT ---------------------------
void MQTTHandler::init() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
}

// -------------------- CONNECT -------------------------
static void ensureConnected() {
    if (!mqttClient.connected()) {
        mqttClient.connect("FireController");
    }
}

// -------------------- UPDATE LOOP ---------------------
void MQTTHandler::update() {
    ensureConnected();
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastPublish >= publishIntervalMs) {
        lastPublish = now;
        MQTTHandler::publishStatus();
    }
}

// -------------------- STATUS JSON ---------------------
void MQTTHandler::publishStatus() {
    JsonDocument doc;

    doc["timestamp"] = millis() / 1000;

    // ---- sensors ----
    auto sensors = doc["sensors"].to<JsonObject>();
    sensors["exhaust_temp_c"] = Sensors::getExhaustTemp();
    sensors["inlet_temp_c"] = Sensors::getInletTemp();
    sensors["inlet_pressure_hpa"] = Sensors::getInletPressure();

    // ---- safety ----
    auto safety = doc["safety"].to<JsonObject>();
    safety["state"] = Safety::isOverheat() ? "overheat" :
                      Safety::isSensorFault() ? "sensor_fault" : "ok";
    safety["overheat"] = Safety::isOverheat();
    safety["sensor_fault"] = Safety::isSensorFault();

    // ---- system ----
    auto system = doc["system"].to<JsonObject>();
    system["uptime_s"] = millis() / 1000;
    system["heap_free"] = ESP.getFreeHeap();
    system["wifi_rssi"] = WiFi.RSSI();
    system["mqtt_connected"] = mqttClient.connected();

    // ---- input ----
    auto input = doc["input"].to<JsonObject>();
    input["button_pressed"] = Button::isPressed();

    // ---- serialize ----
    char buffer[512];
    size_t len = serializeJson(doc, buffer);

    mqttClient.publish("firecontroller/status", buffer, len);
}

// -------------------- CONNECTION STATUS ---------------
bool MQTTHandler::isConnected() {
    return mqttClient.connected();
}
