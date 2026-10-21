#include "drivers.h"

#ifdef ARDUINO
#include <Arduino.h>

static int g_buzzer_pin = 19;
static uint32_t g_last_toggle_ms = 0;
static bool g_buzzer_state = false;

void initBuzzer(int buzzer_pin) {
    g_buzzer_pin = buzzer_pin;
    pinMode(g_buzzer_pin, OUTPUT);
    digitalWrite(g_buzzer_pin, LOW);
    g_buzzer_state = false;
}

void setBuzzerDirect(bool active) {
    digitalWrite(g_buzzer_pin, active ? HIGH : LOW);
    g_buzzer_state = active;
}

void updateBuzzer(uint32_t now_ms, AlarmState state) {
    switch (state) {
        case AlarmState::ALARM: {
            // Aggressive siren cadence: 150ms ON, 100ms OFF
            uint32_t period = g_buzzer_state ? 150 : 100;
            if (now_ms - g_last_toggle_ms >= period) {
                g_buzzer_state = !g_buzzer_state;
                digitalWrite(g_buzzer_pin, g_buzzer_state ? HIGH : LOW);
                g_last_toggle_ms = now_ms;
            }
            break;
        }
        case AlarmState::SENSOR_FAULT: {
            // Fault chirp: 50ms chirp every 2500ms
            uint32_t period = g_buzzer_state ? 50 : 2500;
            if (now_ms - g_last_toggle_ms >= period) {
                g_buzzer_state = !g_buzzer_state;
                digitalWrite(g_buzzer_pin, g_buzzer_state ? HIGH : LOW);
                g_last_toggle_ms = now_ms;
            }
            break;
        }
        case AlarmState::SUSPECT:
        case AlarmState::NORMAL:
        default:
            if (g_buzzer_state) {
                digitalWrite(g_buzzer_pin, LOW);
                g_buzzer_state = false;
            }
            break;
    }
}

#else
// Native stub
void initBuzzer(int buzzer_pin) {}
void setBuzzerDirect(bool active) {}
void updateBuzzer(uint32_t now_ms, AlarmState state) {}
#endif
