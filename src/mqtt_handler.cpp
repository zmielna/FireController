#include "mqtt_handler.h"
#include "config.h"
#include "sensors.h"
#include "safety.h"
#include "button.h"
#include "actuator.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static unsigned long lastPublish = 0;
static unsigned long lastWifiAttempt = 0;
static unsigned long lastMqttAttempt = 0;

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, MQTT_TOPIC_SET_INTAKE) != 0) {
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload, length)) {
        return;
    }

    if (doc["open"].is<bool>()) {
        Actuator::setDesired(doc["open"].as<bool>());
    }
}

static void ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    unsigned long now = millis();
    if (now - lastWifiAttempt < WIFI_RETRY_MS) {
        return;
    }
    lastWifiAttempt = now;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void ensureConnected() {
    ensureWiFi();
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (mqttClient.connected()) {
        return;
    }

    unsigned long now = millis();
    if (now - lastMqttAttempt < MQTT_RETRY_MS) {
        return;
    }
    lastMqttAttempt = now;

    if (mqttClient.connect(MQTT_CLIENT_ID)) {
        mqttClient.subscribe(MQTT_TOPIC_SET_INTAKE);
    }
}

void MQTTHandler::init() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(onMqttMessage);
    mqttClient.setBufferSize(512);
}

void MQTTHandler::update() {
    ensureConnected();
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastPublish >= MQTT_PUBLISH_MS) {
        lastPublish = now;
        publishStatus();
    }
}

void MQTTHandler::publishStatus() {
    if (!mqttClient.connected()) {
        return;
    }

    JsonDocument doc;

    doc["timestamp"] = millis() / 1000;

    auto sensors = doc["sensors"].to<JsonObject>();
    sensors["exhaust_temp_c"] = Sensors::getExhaustTemp();
    sensors["inlet_temp_c"] = Sensors::getInletTemp();
    sensors["inlet_pressure_hpa"] = Sensors::getInletPressure();

    auto safety = doc["safety"].to<JsonObject>();
    safety["state"] = Safety::isOverheat() ? "overheat" :
                      Safety::isSensorFault() ? "sensor_fault" : "ok";
    safety["overheat"] = Safety::isOverheat();
    safety["sensor_fault"] = Safety::isSensorFault();

    auto actuator = doc["actuator"].to<JsonObject>();
    actuator["open"] = Actuator::isOpen();
    actuator["desired_open"] = Actuator::isDesiredOpen();
    actuator["safety_blocked"] = Actuator::isSafetyBlocked();

    auto system = doc["system"].to<JsonObject>();
    system["uptime_s"] = millis() / 1000;
    system["heap_free"] = ESP.getFreeHeap();
    system["wifi_rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    system["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    system["mqtt_connected"] = mqttClient.connected();

    auto input = doc["input"].to<JsonObject>();
    input["button_pressed"] = Button::isPressed();

    char buffer[512];
    size_t len = serializeJson(doc, buffer);
    mqttClient.publish(MQTT_TOPIC_STATUS, buffer, len);
}

bool MQTTHandler::isConnected() {
    return mqttClient.connected();
}
