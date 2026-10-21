#include "event.h"
#include <sstream>
#include <iomanip>

TelemetryEvent makeEvent(const DeviceSnapshot& snapshot, const SensorReading& reading, AlarmTransition transition, uint32_t sample_age_ms) {
    TelemetryEvent ev;
    ev.schema_version = 1;
    ev.device_id = snapshot.device_id;
    ev.boot_id = snapshot.boot_id;
    ev.seq = snapshot.seq;
    ev.uptime_ms = snapshot.uptime_ms;
    ev.sample_age_ms = sample_age_ms;

    ev.temperature_valid = reading.temperature_valid;
    ev.temperature_c = reading.temperature_c;

    ev.flame_valid = reading.flame_valid;
    ev.flame_detected = reading.flame_detected;

    ev.sensor_fault = reading.hardware_fault || (!reading.flame_valid && !reading.temperature_valid);
    ev.alarm_state = AlarmLogic::stateToString(snapshot.current_state);
    ev.alarm_latched = snapshot.latched;

    if (transition != AlarmTransition::NONE) {
        ev.event = AlarmLogic::transitionToString(transition);
    } else {
        ev.event = "heartbeat";
    }

    if (snapshot.incident_id > 0) {
        std::ostringstream ss;
        ss << "inc-" << std::setfill('0') << std::setw(6) << snapshot.incident_id;
        ev.incident_id = ss.str();
    } else {
        ev.incident_id = "";
    }

    return ev;
}

std::string eventKey(const TelemetryEvent& event) {
    return event.device_id + ":" + event.boot_id + ":" + std::to_string(event.seq);
}

std::string serializeJson(const TelemetryEvent& event) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"schema_version\":" << event.schema_version << ",";
    ss << "\"device_id\":\"" << event.device_id << "\",";
    ss << "\"boot_id\":\"" << event.boot_id << "\",";
    ss << "\"seq\":" << event.seq << ",";
    ss << "\"uptime_ms\":" << event.uptime_ms << ",";
    ss << "\"sample_age_ms\":" << event.sample_age_ms << ",";

    // Nullable temperature
    if (event.temperature_valid) {
        ss << "\"temperature_c\":" << std::fixed << std::setprecision(1) << event.temperature_c << ",";
    } else {
        ss << "\"temperature_c\":null,";
    }

    // Nullable flame
    if (event.flame_valid) {
        ss << "\"flame_detected\":" << (event.flame_detected ? "true" : "false") << ",";
    } else {
        ss << "\"flame_detected\":null,";
    }

    ss << "\"sensor_fault\":" << (event.sensor_fault ? "true" : "false") << ",";
    ss << "\"alarm_state\":\"" << event.alarm_state << "\",";
    ss << "\"alarm_latched\":" << (event.alarm_latched ? "true" : "false") << ",";
    ss << "\"event\":\"" << event.event << "\",";

    if (!event.incident_id.empty()) {
        ss << "\"incident_id\":\"" << event.incident_id << "\"";
    } else {
        ss << "\"incident_id\":null";
    }

    ss << "}";
    return ss.str();
}
