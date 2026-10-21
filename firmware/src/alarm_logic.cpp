#include "alarm_logic.h"

AlarmLogic::AlarmLogic(const AlarmConfig& config)
    : m_config(config),
      m_state(AlarmState::NORMAL),
      m_latched(false),
      m_incident_id(0),
      m_suspect_active(false),
      m_suspect_start_ms(0),
      m_fault_active(false),
      m_fault_start_ms(0),
      m_clear_condition_start_ms(0),
      m_in_clear_window(false),
      m_manual_ack_requested(false) {}

void AlarmLogic::restoreLatchedState(bool latched, uint32_t incident_id) {
    m_latched = latched;
    if (latched) {
        m_state = AlarmState::ALARM;
        m_incident_id = (incident_id > 0) ? incident_id : 1;
    }
}

void AlarmLogic::acknowledgeLocalClear(uint32_t now_ms) {
    m_manual_ack_requested = true;
}

const char* AlarmLogic::stateToString(AlarmState s) {
    switch (s) {
        case AlarmState::NORMAL: return "NORMAL";
        case AlarmState::SUSPECT: return "SUSPECT";
        case AlarmState::ALARM: return "ALARM";
        case AlarmState::SENSOR_FAULT: return "SENSOR_FAULT";
        default: return "UNKNOWN";
    }
}

const char* AlarmLogic::transitionToString(AlarmTransition t) {
    switch (t) {
        case AlarmTransition::NONE: return "NONE";
        case AlarmTransition::ALARM_STARTED: return "alarm_started";
        case AlarmTransition::ALARM_CLEARED: return "alarm_cleared";
        case AlarmTransition::SENSOR_FAULT_STARTED: return "sensor_fault";
        case AlarmTransition::SENSOR_FAULT_CLEARED: return "sensor_recovered";
        default: return "unknown";
    }
}

AlarmTransition AlarmLogic::update(const SensorReading& reading, uint32_t now_ms) {
    // 1. Hardware / sensor fault handling
    bool is_fault = reading.hardware_fault || (!reading.flame_valid && !reading.temperature_valid);

    if (is_fault) {
        if (!m_fault_active) {
            m_fault_active = true;
            m_fault_start_ms = now_ms;
        }

        if (m_state != AlarmState::ALARM) { // Do not mask an active alarm with sensor fault
            if ((now_ms - m_fault_start_ms) >= m_config.sensor_fault_ms && m_state != AlarmState::SENSOR_FAULT) {
                m_state = AlarmState::SENSOR_FAULT;
                return AlarmTransition::SENSOR_FAULT_STARTED;
            }
        }
        return AlarmTransition::NONE;
    } else {
        if (m_state == AlarmState::SENSOR_FAULT) {
            m_fault_active = false;
            m_state = AlarmState::NORMAL;
            return AlarmTransition::SENSOR_FAULT_CLEARED;
        }
        m_fault_active = false;
    }

    // 2. Fire condition evaluation
    bool flame_trigger = reading.flame_valid && reading.flame_detected;
    bool temp_trigger = reading.temperature_valid && (reading.temperature_c >= m_config.temp_alarm_c);
    bool fire_condition = flame_trigger || temp_trigger;

    // 3. Alarm State Transitions
    if (m_state == AlarmState::ALARM || m_latched) {
        // While in ALARM, check if conditions are safe enough to allow clearing
        bool flame_safe = reading.flame_valid && !reading.flame_detected;
        bool temp_safe = reading.temperature_valid && (reading.temperature_c <= m_config.temp_clear_c);

        if (flame_safe && temp_safe) {
            if (!m_in_clear_window) {
                m_in_clear_window = true;
                m_clear_condition_start_ms = now_ms;
            } else if ((now_ms - m_clear_condition_start_ms) >= m_config.clear_ms) {
                // Clear conditions sustained long enough
                // Requires either local acknowledgement or clear window completion
                m_latched = false;
                m_state = AlarmState::NORMAL;
                m_in_clear_window = false;
                m_suspect_active = false;
                m_manual_ack_requested = false;
                return AlarmTransition::ALARM_CLEARED;
            }
        } else {
            // Unsafe conditions resumed, reset clear window
            m_in_clear_window = false;
        }
        return AlarmTransition::NONE;
    }

    // Currently NORMAL or SUSPECT
    if (fire_condition) {
        if (!m_suspect_active) {
            m_suspect_active = true;
            m_suspect_start_ms = now_ms;
            m_state = AlarmState::SUSPECT;
        } else {
            if ((now_ms - m_suspect_start_ms) >= m_config.confirm_ms) {
                // Confirmed alarm!
                m_state = AlarmState::ALARM;
                m_latched = true;
                m_incident_id++;
                m_suspect_active = false;
                m_in_clear_window = false;
                return AlarmTransition::ALARM_STARTED;
            }
        }
    } else {
        if (m_suspect_active) {
            // False alarm or transient noise before confirmation threshold
            m_suspect_active = false;
            m_state = AlarmState::NORMAL;
        }
    }

    return AlarmTransition::NONE;
}
