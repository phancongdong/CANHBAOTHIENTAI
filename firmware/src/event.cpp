#include "event.h"
#include <sstream>
#include <iomanip>

TelemetryEvent makeEvent(
    const DeviceSnapshot& snapshot,
    const SensorReading& reading,
    const DisasterEvaluation& eval,
    uint32_t sample_age_ms
) {
    TelemetryEvent ev;
    ev.schema_version = 1;
    ev.device_id = snapshot.device_id;
    ev.boot_id = snapshot.boot_id;
    ev.seq = snapshot.seq;
    ev.uptime_ms = snapshot.uptime_ms;
    ev.sample_age_ms = sample_age_ms;

    ev.vibration_g = reading.vibration_g;
    ev.flame_raw = reading.flame_raw;
    ev.temperature_c = reading.temperature_c;
    ev.sensor_fault = !reading.connection_ok;
    ev.lcd_message = eval.lcd_line1;

    if (eval.is_emergency) {
        ev.alarm_state = "ALARM";
        ev.alarm_latched = true;
    } else if (eval.is_warning) {
        ev.alarm_state = "SUSPECT";
        ev.alarm_latched = false;
    } else if (!reading.connection_ok) {
        ev.alarm_state = "SENSOR_FAULT";
        ev.alarm_latched = false;
    } else {
        ev.alarm_state = "NORMAL";
        ev.alarm_latched = false;
    }

    ev.event = eval.alert_event;
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
    ss << "\"vibration_g\":" << std::fixed << std::setprecision(2) << event.vibration_g << ",";
    ss << "\"flame_raw\":" << event.flame_raw << ",";
    ss << "\"temperature_c\":" << std::fixed << std::setprecision(1) << event.temperature_c << ",";
    ss << "\"flame_detected\":" << (event.flame_raw < 1600 ? "true" : "false") << ",";
    ss << "\"sensor_fault\":" << (event.sensor_fault ? "true" : "false") << ",";
    ss << "\"alarm_state\":\"" << event.alarm_state << "\",";
    ss << "\"alarm_latched\":" << (event.alarm_latched ? "true" : "false") << ",";
    ss << "\"event\":\"" << event.event << "\",";
    ss << "\"lcd_message\":\"" << event.lcd_message << "\",";

    if (!event.incident_id.empty()) {
        ss << "\"incident_id\":\"" << event.incident_id << "\"";
    } else {
        ss << "\"incident_id\":null";
    }

    ss << "}";
    return ss.str();
}
