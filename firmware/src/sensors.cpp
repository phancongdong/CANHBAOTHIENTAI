#include "drivers.h"
#include <cmath>

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

static int g_flame_pin = 34;
static int g_temp_pin = 4;
static OneWire* g_oneWire = nullptr;
static DallasTemperature* g_tempSensors = nullptr;
static Adafruit_MPU6050 g_mpu;
static bool g_mpu_connected = false;

void initSensors(int flame_pin, int temp_pin, int sda_pin, int scl_pin) {
    g_flame_pin = flame_pin;
    g_temp_pin = temp_pin;

    pinMode(g_flame_pin, INPUT);

    // 1. Init DS18B20 Temperature
    g_oneWire = new OneWire(g_temp_pin);
    g_tempSensors = new DallasTemperature(g_oneWire);
    g_tempSensors->begin();
    g_tempSensors->setWaitForConversion(false);
    g_tempSensors->requestTemperatures();

    // 2. Init I2C and MPU6050
    Wire.begin(sda_pin, scl_pin);
    if (g_mpu.begin()) {
        g_mpu_connected = true;
        g_mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        g_mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println("[SENSOR] MPU6050 initialized successfully.");
    } else {
        g_mpu_connected = false;
        Serial.println("[SENSOR] WARNING: MPU6050 not detected!");
    }
}

SensorReading readSensors() {
    SensorReading r;
    bool conn_error = false;

    // 1. Đọc Rung Động / Gia Tốc (RĐ)
    if (g_mpu_connected) {
        sensors_event_t a, g, temp;
        if (g_mpu.getEvent(&a, &g, &temp)) {
            // Gia tốc tổng hợp: a_total = sqrt(ax^2 + ay^2 + az^2) / 9.80665 (đơn vị g)
            float total_acc_ms2 = std::sqrt(a.acceleration.x * a.acceleration.x +
                                            a.acceleration.y * a.acceleration.y +
                                            a.acceleration.z * a.acceleration.z);
            float current_g = total_acc_ms2 / 9.80665f;
            // Rung động là độ lệch so với trọng lực tĩnh 1.0g
            r.vibration_g = std::abs(current_g - 1.0f);
        } else {
            conn_error = true;
            r.vibration_g = 0.0f;
        }
    } else {
        conn_error = true;
        r.vibration_g = 0.0f;
    }

    // 2. Đọc Ngọn Lửa (NL - ADC: 0 - 4095) có lọc lấy mẫu trung bình 8 lần siêu nhạy
    long flame_sum = 0;
    for (int i = 0; i < 8; i++) {
        flame_sum += analogRead(g_flame_pin);
        delayMicroseconds(50);
    }
    r.flame_raw = (int)(flame_sum / 8);

    // 3. Đọc Nhiệt Độ (NĐ - °C)
    if (g_tempSensors) {
        float tempC = g_tempSensors->getTempCByIndex(0);
        g_tempSensors->requestTemperatures();

        if (tempC <= -120.0f || tempC > 130.0f) {
            conn_error = true;
            r.temperature_c = 0.0f;
        } else {
            r.temperature_c = tempC;
        }
    } else {
        conn_error = true;
        r.temperature_c = 0.0f;
    }

    // Đánh giá kiểm tra kết nối (Kiểm Tra Kết Nối: Có / Không)
    r.connection_ok = !conn_error;

    return r;
}

#else
// Native mock fallback for host testing
static SensorReading g_mock_reading;

void initSensors(int flame_pin, int temp_pin, int sda_pin, int scl_pin) {
    g_mock_reading.vibration_g = 0.05f;
    g_mock_reading.flame_raw = 3800;
    g_mock_reading.temperature_c = 28.0f;
    g_mock_reading.connection_ok = true;
}

SensorReading readSensors() {
    return g_mock_reading;
}
#endif
