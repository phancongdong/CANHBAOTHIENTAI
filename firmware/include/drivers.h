#ifndef DRIVERS_H
#define DRIVERS_H

#include <cstdint>
#include <string>
#include "alarm_logic.h"
#include "event.h"

// Kết quả gửi mạng
enum class TransportResult {
    SENT,
    DUPLICATE_ACK,
    RETRYABLE_FAILURE,
    PERMANENT_FAILURE
};

// 1. Cảm biến (Gia tốc RĐ, Lửa NL, Nhiệt độ NĐ)
void initSensors(int flame_pin, int temp_pin, int sda_pin, int scl_pin);
SensorReading readSensors();

// 2. Còi & LED (OFF, 1Hz, 10Hz)
void initAlertOutputs(int buzzer_pin, int led_pin);
void updateAlertOutputs(uint32_t now_ms, CadenceMode cadence);
void setAlertOutputsDirect(bool buzzer_on, bool led_on);
void testBuzzer(int beeps = 2, int on_ms = 100, int off_ms = 80);

// 3. Màn hình LCD 1602 I2C
void initLcd(uint8_t i2c_addr, int sda_pin, int scl_pin);
void updateLcd(const std::string& line1, const std::string& line2);

// 4. HTTP Transport gửi VPS
void initTransport(const char* host, int port, const char* bearer_token);
TransportResult postEvent(const TelemetryEvent& event);

#endif // DRIVERS_H
