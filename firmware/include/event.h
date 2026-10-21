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
    AlarmState current_state = AlarmState::NORMAL;
    bool latched = false;
    uint32_t incident_id = 0;
};

struct TelemetryEvent {
    int schema_version = 1;
    std::string device_id;
    std::string boot_id;
    uint32_t seq = 0;
    uint32_t uptime_ms = 0;
    uint32_t sample_age_ms = 0;

    bool temperature_valid = true;
    float temperature_c = 0.0f;

    bool flame_valid = true;
    bool flame_detected = false;

    bool sensor_fault = false;
    std::string alarm_state = "NORMAL";
    bool alarm_latched = false;
    std::string event = "heartbeat";
    std::string incident_id; // empty or "inc-XXXXXX"
};

TelemetryEvent makeEvent(const DeviceSnapshot& snapshot, const SensorReading& reading, AlarmTransition transition, uint32_t sample_age_ms = 0);
std::string serializeJson(const TelemetryEvent& event);
std::string eventKey(const TelemetryEvent& event);

#endif // EVENT_H
