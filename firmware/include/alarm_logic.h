#ifndef ALARM_LOGIC_H
#define ALARM_LOGIC_H

#include <cstdint>
#include <string>

enum class AlarmState {
    NORMAL,
    SUSPECT,
    ALARM,
    SENSOR_FAULT
};

enum class AlarmTransition {
    NONE,
    ALARM_STARTED,
    ALARM_CLEARED,
    SENSOR_FAULT_STARTED,
    SENSOR_FAULT_CLEARED
};

struct SensorReading {
    bool flame_valid = true;
    bool flame_detected = false;
    bool temperature_valid = true;
    float temperature_c = 25.0f;
    bool hardware_fault = false;
};

struct AlarmConfig {
    float temp_warn_c = 45.0f;
    float temp_alarm_c = 55.0f;
    float temp_clear_c = 38.0f;
    uint32_t confirm_ms = 1000;
    uint32_t clear_ms = 10000;
    uint32_t sensor_fault_ms = 3000;
};

class AlarmLogic {
public:
    explicit AlarmLogic(const AlarmConfig& config);

    AlarmTransition update(const SensorReading& reading, uint32_t now_ms);
    AlarmState state() const { return m_state; }
    bool latched() const { return m_latched; }
    uint32_t incidentId() const { return m_incident_id; }

    void acknowledgeLocalClear(uint32_t now_ms);
    void restoreLatchedState(bool latched, uint32_t incident_id);

    static const char* stateToString(AlarmState s);
    static const char* transitionToString(AlarmTransition t);

private:
    AlarmConfig m_config;
    AlarmState m_state = AlarmState::NORMAL;
    bool m_latched = false;
    uint32_t m_incident_id = 0;

    // Timing tracking
    bool m_suspect_active = false;
    uint32_t m_suspect_start_ms = 0;

    bool m_fault_active = false;
    uint32_t m_fault_start_ms = 0;

    uint32_t m_clear_condition_start_ms = 0;
    bool m_in_clear_window = false;

    bool m_manual_ack_requested = false;
};

#endif // ALARM_LOGIC_H
