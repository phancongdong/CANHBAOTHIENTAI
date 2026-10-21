#include <cassert>
#include <iostream>
#include "../include/alarm_logic.h"

static SensorReading makeReading(bool flame_valid, bool flame_det, bool temp_valid, float temp_c, bool fault = false) {
    SensorReading r;
    r.flame_valid = flame_valid;
    r.flame_detected = flame_det;
    r.temperature_valid = temp_valid;
    r.temperature_c = temp_c;
    r.hardware_fault = fault;
    return r;
}

static AlarmConfig testConfig() {
    AlarmConfig cfg;
    cfg.temp_warn_c = 45.0f;
    cfg.temp_alarm_c = 55.0f;
    cfg.temp_clear_c = 38.0f;
    cfg.confirm_ms = 1000;
    cfg.clear_ms = 5000;
    cfg.sensor_fault_ms = 2000;
    return cfg;
}

void test_short_flame_pulse_does_not_start_alarm() {
    AlarmLogic logic(testConfig());
    assert(logic.update(makeReading(true, false, true, 25.0f), 0) == AlarmTransition::NONE);
    // Pulse begins
    assert(logic.update(makeReading(true, true, true, 25.0f), 100) == AlarmTransition::NONE);
    assert(logic.state() == AlarmState::SUSPECT);
    assert(logic.update(makeReading(true, true, true, 25.0f), 999) == AlarmTransition::NONE);
    // Pulse ends before confirm_ms (1000ms)
    assert(logic.update(makeReading(true, false, true, 25.0f), 1000) == AlarmTransition::NONE);
    assert(logic.state() == AlarmState::NORMAL);
    assert(!logic.latched());
    std::cout << "[PASS] test_short_flame_pulse_does_not_start_alarm\n";
}

void test_confirmed_flame_starts_latched_alarm() {
    AlarmLogic logic(testConfig());
    assert(logic.update(makeReading(true, true, true, 25.0f), 0) == AlarmTransition::NONE);
    assert(logic.update(makeReading(true, true, true, 25.0f), 500) == AlarmTransition::NONE);
    // Reached confirm_ms (1000ms)
    assert(logic.update(makeReading(true, true, true, 25.0f), 1000) == AlarmTransition::ALARM_STARTED);
    assert(logic.state() == AlarmState::ALARM);
    assert(logic.latched());
    assert(logic.incidentId() > 0);
    std::cout << "[PASS] test_confirmed_flame_starts_latched_alarm\n";
}

void test_hysteresis_and_safe_clear() {
    AlarmLogic logic(testConfig());
    // Trigger alarm by temperature >= 55C
    assert(logic.update(makeReading(true, false, true, 60.0f), 0) == AlarmTransition::NONE);
    assert(logic.update(makeReading(true, false, true, 60.0f), 1000) == AlarmTransition::ALARM_STARTED);
    assert(logic.state() == AlarmState::ALARM);

    // Temperature drops to 40C (below alarm 55C, but above clear 38C)
    assert(logic.update(makeReading(true, false, true, 40.0f), 2000) == AlarmTransition::NONE);
    assert(logic.state() == AlarmState::ALARM); // Stays in alarm due to hysteresis

    // Temperature drops to 35C (below clear 38C)
    assert(logic.update(makeReading(true, false, true, 35.0f), 3000) == AlarmTransition::NONE);
    assert(logic.state() == AlarmState::ALARM); // Clear window started, needs clear_ms (5000ms)

    assert(logic.update(makeReading(true, false, true, 35.0f), 7000) == AlarmTransition::NONE);
    assert(logic.state() == AlarmState::ALARM); // Only 4000ms passed

    // 5000ms completed at 8000ms
    assert(logic.update(makeReading(true, false, true, 35.0f), 8000) == AlarmTransition::ALARM_CLEARED);
    assert(logic.state() == AlarmState::NORMAL);
    assert(!logic.latched());
    std::cout << "[PASS] test_hysteresis_and_safe_clear\n";
}

void test_sensor_fault_and_recovery() {
    AlarmLogic logic(testConfig());
    // Single sensor fault does not trigger SENSOR_FAULT state if other is valid
    assert(logic.update(makeReading(false, false, true, 25.0f), 0) == AlarmTransition::NONE);
    assert(logic.state() == AlarmState::NORMAL);

    // Both sensors invalid
    assert(logic.update(makeReading(false, false, false, 0.0f), 100) == AlarmTransition::NONE);
    assert(logic.update(makeReading(false, false, false, 0.0f), 1500) == AlarmTransition::NONE);
    // After sensor_fault_ms (2000ms)
    assert(logic.update(makeReading(false, false, false, 0.0f), 2200) == AlarmTransition::SENSOR_FAULT_STARTED);
    assert(logic.state() == AlarmState::SENSOR_FAULT);

    // Recovery
    assert(logic.update(makeReading(true, false, true, 25.0f), 2500) == AlarmTransition::SENSOR_FAULT_CLEARED);
    assert(logic.state() == AlarmState::NORMAL);
    std::cout << "[PASS] test_sensor_fault_and_recovery\n";
}

void test_persisted_latched_state_restoration() {
    AlarmLogic logic(testConfig());
    // Simulate reboot restoring latched state from NVS
    logic.restoreLatchedState(true, 42);
    assert(logic.state() == AlarmState::ALARM);
    assert(logic.latched());
    assert(logic.incidentId() == 42);
    std::cout << "[PASS] test_persisted_latched_state_restoration\n";
}

int main() {
    std::cout << "Running AlarmLogic Unit Tests...\n";
    test_short_flame_pulse_does_not_start_alarm();
    test_confirmed_flame_starts_latched_alarm();
    test_hysteresis_and_safe_clear();
    test_sensor_fault_and_recovery();
    test_persisted_latched_state_restoration();
    std::cout << "All AlarmLogic tests passed successfully!\n";
    return 0;
}
