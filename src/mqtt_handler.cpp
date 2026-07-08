#include "mqtt_handler.h"
#include "sensors.h"
#include "safety.h"
#include "button.h"
#include <ArduinoJson.h>

void MQTTHandler::publishStatus() {
    StaticJsonDocument<256> doc;

    doc["timestamp"] = millis() / 1000;

    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["exhaust_temp_c"] = Sensors::getExhaustTemp();
    sensors["inlet_pressure_hpa"] = Sensors::getInletPressure();

    JsonObject safety = doc.createNestedObject("safety");
    safety["state"] = Safety::isOverheat() ? "overheat" :
                      Safety::isSensorFault() ? "sensor_fault" : "ok";
    safety["overheat"] = Safety::isOverheat();
    safety["sensor_fault"] = Safety::isSensorFault();

    JsonObject system = doc.createNestedObject("system");
    system["uptime_s"] = millis() / 1000;
    system["heap_free"] = ESP.getFreeHeap();
    system["wifi_rssi"] = WiFi.RSSI();
    system["mqtt_connected"] = true; // replace with real check

    JsonObject input = doc.createNestedObject("input");
    input["button_pressed"] = Button::isPressed();

    char buffer[256];
    size_t len = serializeJson(doc, buffer);

    mqttClient.publish("firecontroller/status", buffer, len);
}
