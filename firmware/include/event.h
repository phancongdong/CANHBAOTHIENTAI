#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <cstdint>
#include "alarm_logic.h"

struct DeviceSnapshot {
    std::string device_id;
    std::string boot_id;
    uint32_t seq = 0;
    uint32_t uptime_ms = 0;
    std::string alert_event = "heartbeat";
    std::string lcd_line1 = "An Toan";
    bool is_emergency = false;
};

struct TelemetryEvent {
    int schema_version = 1;
    std::string device_id;
    std::string boot_id;
    uint32_t seq = 0;
    uint32_t uptime_ms = 0;
    uint32_t sample_age_ms = 0;

    float vibration_g = 0.0f;
    int flame_raw = 4095;
    float temperature_c = 25.0f;

    bool sensor_fault = false;
    std::string alarm_state = "NORMAL";
    bool alarm_latched = false;
    std::string event = "heartbeat";
    std::string lcd_message = "An Toan";
    std::string incident_id;
};

TelemetryEvent makeEvent(
    const DeviceSnapshot& snapshot,
    const SensorReading& reading,
    const DisasterEvaluation& eval,
    uint32_t sample_age_ms = 0
);

std::string serializeJson(const TelemetryEvent& event);
std::string eventKey(const TelemetryEvent& event);

#endif // EVENT_H
