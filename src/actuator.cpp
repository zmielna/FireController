#include "actuator.h"
#include "config.h"
#include "safety.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#include <math.h>

static Servo damperServo;
static float desiredPosition = STARTUP_POSITION_PCT;
static float currentPosition = STARTUP_POSITION_PCT;
static bool safetyOverridden = false;

static void applyPosition(float pct) {
    pct = constrain(pct, 0.0f, 100.0f);
    currentPosition = pct;
    float us = SERVO_MIN_US + (SERVO_MAX_US - SERVO_MIN_US) * (pct / 100.0f);
    damperServo.writeMicroseconds((int)us);
}

void Actuator::init() {
    damperServo.setPeriodHertz(50);
    damperServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    // Fail-open on boot: no sensor data has been read yet, so there is no
    // basis for restricting airflow. Mirrors the SENSOR_FAULT behavior.
    applyPosition(STARTUP_POSITION_PCT);
}

void Actuator::update() {
    float target = desiredPosition;

    // Soft ceiling first (e.g. HIGH_TEMP_LIMIT) - clamps but doesn't override
    // a lower value the user/HA may have already requested.
    float ceiling = Safety::getPositionCeiling();
    if (ceiling >= 0.0f && target > ceiling) {
        target = ceiling;
    }

    // Hard override last (e.g. CRITICAL_TEMP, SENSOR_FAULT, MQTT_LOST) -
    // always wins regardless of what was requested.
    float enforced = Safety::getEnforcedPosition();
    if (enforced >= 0.0f) {
        target = enforced;
    }

    safetyOverridden = fabsf(target - desiredPosition) > 0.5f;
    applyPosition(target);
}

void Actuator::setDesiredPosition(float percent) {
    desiredPosition = constrain(percent, 0.0f, 100.0f);
}

float Actuator::getDesiredPosition() { return desiredPosition; }
float Actuator::getCurrentPosition() { return currentPosition; }
bool  Actuator::isSafetyOverridden() { return safetyOverridden; }
