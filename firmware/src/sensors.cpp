#include "drivers.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

static int g_flame_pin = 4;
static int g_temp_pin = 18;
static OneWire* g_oneWire = nullptr;
static DallasTemperature* g_tempSensors = nullptr;

void initSensors(int flame_pin, int temp_pin) {
    g_flame_pin = flame_pin;
    g_temp_pin = temp_pin;
    pinMode(g_flame_pin, INPUT);

    g_oneWire = new OneWire(g_temp_pin);
    g_tempSensors = new DallasTemperature(g_oneWire);
    g_tempSensors->begin();
    g_tempSensors->setWaitForConversion(false); // Non-blocking temperature conversion
    g_tempSensors->requestTemperatures();
}

SensorReading readSensors() {
    SensorReading r;

    // Read Flame Digital Output (Active LOW on most optical flame modules)
    // LOW = Flame present, HIGH = No flame
    int flame_val = digitalRead(g_flame_pin);
    r.flame_valid = true;
    r.flame_detected = (flame_val == LOW);

    // Read DS18B20 Temperature
    if (g_tempSensors) {
        float tempC = g_tempSensors->getTempCByIndex(0);
        g_tempSensors->requestTemperatures(); // Trigger next conversion

        // DS18B20 error codes: DEVICE_DISCONNECTED_C = -127.0f, 85.0f = power-on reset unread
        if (tempC <= -120.0f || tempC > 130.0f) {
            r.temperature_valid = false;
            r.temperature_c = 0.0f;
        } else {
            r.temperature_valid = true;
            r.temperature_c = tempC;
        }
    } else {
        r.temperature_valid = false;
    }

    r.hardware_fault = (!r.flame_valid && !r.temperature_valid);
    return r;
}

#else
// Native mock fallback for host compilation / unit tests
static SensorReading g_mock_reading;

void initSensors(int flame_pin, int temp_pin) {
    g_mock_reading.flame_valid = true;
    g_mock_reading.flame_detected = false;
    g_mock_reading.temperature_valid = true;
    g_mock_reading.temperature_c = 25.0f;
    g_mock_reading.hardware_fault = false;
}

SensorReading readSensors() {
    return g_mock_reading;
}
#endif
