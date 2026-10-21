#include <cassert>
#include <iostream>
#include "../include/event.h"

void test_event_serialization_preserves_nullable_sensor_values() {
    DeviceSnapshot snap;
    snap.device_id = "kitchen-01";
    snap.boot_id = "boot-000001";
    snap.seq = 7;
    snap.uptime_ms = 12000;
    snap.current_state = AlarmState::SENSOR_FAULT;
    snap.latched = false;
    snap.incident_id = 0;

    SensorReading reading;
    reading.temperature_valid = false;
    reading.temperature_c = 0.0f;
    reading.flame_valid = false;
    reading.flame_detected = false;
    reading.hardware_fault = true;

    TelemetryEvent ev = makeEvent(snap, reading, AlarmTransition::SENSOR_FAULT_STARTED, 10);
    std::string json = serializeJson(ev);

    assert(json.find("\"temperature_c\":null") != std::string::npos);
    assert(json.find("\"flame_detected\":null") != std::string::npos);
    assert(json.find("\"sensor_fault\":true") != std::string::npos);
    assert(json.find("\"event\":\"sensor_fault\"") != std::string::npos);
    assert(json.find("\"incident_id\":null") != std::string::npos);
    std::cout << "[PASS] test_event_serialization_preserves_nullable_sensor_values\n";
}

void test_event_key_is_unique_per_boot_and_sequence() {
    TelemetryEvent ev1;
    ev1.device_id = "kitchen-01";
    ev1.boot_id = "b1";
    ev1.seq = 1;

    TelemetryEvent ev2;
    ev2.device_id = "kitchen-01";
    ev2.boot_id = "b1";
    ev2.seq = 2;

    TelemetryEvent ev3;
    ev3.device_id = "kitchen-01";
    ev3.boot_id = "b2";
    ev3.seq = 1;

    assert(eventKey(ev1) != eventKey(ev2));
    assert(eventKey(ev1) != eventKey(ev3));
    assert(eventKey(ev1) == "kitchen-01:b1:1");
    std::cout << "[PASS] test_event_key_is_unique_per_boot_and_sequence\n";
}

void test_valid_alarm_serialization() {
    DeviceSnapshot snap;
    snap.device_id = "kitchen-01";
    snap.boot_id = "boot-000017";
    snap.seq = 42;
    snap.uptime_ms = 123456;
    snap.current_state = AlarmState::ALARM;
    snap.latched = true;
    snap.incident_id = 42;

    SensorReading reading;
    reading.temperature_valid = true;
    reading.temperature_c = 48.2f;
    reading.flame_valid = true;
    reading.flame_detected = true;
    reading.hardware_fault = false;

    TelemetryEvent ev = makeEvent(snap, reading, AlarmTransition::ALARM_STARTED, 0);
    std::string json = serializeJson(ev);

    assert(json.find("\"schema_version\":1") != std::string::npos);
    assert(json.find("\"device_id\":\"kitchen-01\"") != std::string::npos);
    assert(json.find("\"temperature_c\":48.2") != std::string::npos);
    assert(json.find("\"flame_detected\":true") != std::string::npos);
    assert(json.find("\"alarm_state\":\"ALARM\"") != std::string::npos);
    assert(json.find("\"alarm_latched\":true") != std::string::npos);
    assert(json.find("\"incident_id\":\"inc-000042\"") != std::string::npos);
    std::cout << "[PASS] test_valid_alarm_serialization\n";
}

int main() {
    std::cout << "Running Event Unit Tests...\n";
    test_event_serialization_preserves_nullable_sensor_values();
    test_event_key_is_unique_per_boot_and_sequence();
    test_valid_alarm_serialization();
    std::cout << "All Event tests passed successfully!\n";
    return 0;
}
