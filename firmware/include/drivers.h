#ifndef DRIVERS_H
#define DRIVERS_H

#include "alarm_logic.h"
#include "event.h"

// Buzzer Cadence Modes
enum class BuzzerPattern {
    OFF,
    ALARM_CADENCE,      // Aggressive alternating beep
    FAULT_CADENCE       // Short chirp every 2 seconds
};

// Transport Result
enum class TransportResult {
    SENT,
    DUPLICATE_ACK,
    RETRYABLE_FAILURE,  // Network timeout, 503, 500
    PERMANENT_FAILURE   // 401 Unauthorized, 422 Invalid Schema
};

// Sensors interface
void initSensors(int flame_pin, int temp_pin);
SensorReading readSensors();

// Buzzer interface
void initBuzzer(int buzzer_pin);
void updateBuzzer(uint32_t now_ms, AlarmState state);
void setBuzzerDirect(bool active);

// Transport interface
void initTransport(const char* host, int port, const char* ca_cert, const char* bearer_token);
TransportResult postEvent(const TelemetryEvent& event);

#endif // DRIVERS_H
