#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.example.h"
#include "alarm_logic.h"
#include "event.h"
#include "event_queue.h"
#include "drivers.h"

static Preferences prefs;
static AlarmLogic alarmLogic(AlarmConfig{
    TEMP_WARN_C,
    TEMP_ALARM_C,
    TEMP_CLEAR_C,
    CONFIRM_MS,
    CLEAR_MS,
    SENSOR_FAULT_MS
});

static EventQueue eventQueue(50);
static DeviceSnapshot snapshot;

static uint32_t last_sensor_read_ms = 0;
static uint32_t last_heartbeat_ms = 0;
static uint32_t last_transport_attempt_ms = 0;
static uint32_t transport_backoff_ms = 1000;

static const uint32_t SENSOR_INTERVAL_MS = 100;
static const uint32_t HEARTBEAT_INTERVAL = HEARTBEAT_INTERVAL_MS;

static int last_button_state = HIGH;
static uint32_t last_debounce_time = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SYSTEM] ESP32 Fire Alarm System Starting...");

    // Initialize Preferences in NVS
    prefs.begin("fire_alarm", false);
    uint32_t boot_count = prefs.getUInt("boot_count", 0) + 1;
    prefs.putUInt("boot_count", boot_count);

    char boot_buf[32];
    snprintf(boot_buf, sizeof(boot_buf), "boot-%06u", boot_count);
    snapshot.device_id = DEVICE_ID;
    snapshot.boot_id = boot_buf;
    snapshot.seq = 0;

    // Restore latched state if previously in ALARM
    bool saved_latched = prefs.getBool("latched", false);
    uint32_t saved_incident = prefs.getUInt("incident_id", 0);
    if (saved_latched) {
        Serial.printf("[SYSTEM] Restoring latched ALARM state (Incident: %u)\n", saved_incident);
        alarmLogic.restoreLatchedState(true, saved_incident);
        snapshot.latched = true;
        snapshot.incident_id = saved_incident;
        snapshot.current_state = AlarmState::ALARM;
    }

    // Initialize Drivers
    initSensors(PIN_FLAME_DO, PIN_ONEWIRE_TEMP);
    initBuzzer(PIN_BUZZER);
    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);

    initTransport(SERVER_HOST, SERVER_PORT, ROOT_CA_CERT, DEVICE_BEARER_TOKEN);

    // Connect to Wi-Fi non-blocking or simple connect
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[WIFI] Connecting to SSID: %s\n", WIFI_SSID);
}

void loop() {
    uint32_t now = millis();
    snapshot.uptime_ms = now;

    // 1. Reset Button Debounce
    int reading = digitalRead(PIN_RESET_BUTTON);
    if (reading != last_button_state) {
        last_debounce_time = now;
    }
    if ((now - last_debounce_time) > 50) {
        if (reading == LOW && last_button_state == HIGH) {
            Serial.println("[BUTTON] Local Clear Button Pressed.");
            alarmLogic.acknowledgeLocalClear(now);
        }
    }
    last_button_state = reading;

    // 2. Sensor Read & State Machine Update (Every 100ms)
    if (now - last_sensor_read_ms >= SENSOR_INTERVAL_MS) {
        last_sensor_read_ms = now;
        SensorReading sr = readSensors();
        AlarmTransition trans = alarmLogic.update(sr, now);

        snapshot.current_state = alarmLogic.state();
        snapshot.latched = alarmLogic.latched();
        snapshot.incident_id = alarmLogic.incidentId();

        // Update buzzer cadence based on state (Runs locally even if network is dead)
        updateBuzzer(now, snapshot.current_state);

        // Save latched state to NVS on state change
        if (trans == AlarmTransition::ALARM_STARTED) {
            Serial.printf("[ALARM] *** FIRE ALARM TRIGGERED (Incident: %u) ***\n", snapshot.incident_id);
            prefs.putBool("latched", true);
            prefs.putUInt("incident_id", snapshot.incident_id);

            snapshot.seq++;
            TelemetryEvent ev = makeEvent(snapshot, sr, trans, 0);
            eventQueue.push(ev);
        } else if (trans == AlarmTransition::ALARM_CLEARED) {
            Serial.println("[ALARM] Alarm cleared safely.");
            prefs.putBool("latched", false);

            snapshot.seq++;
            TelemetryEvent ev = makeEvent(snapshot, sr, trans, 0);
            eventQueue.push(ev);
        } else if (trans == AlarmTransition::SENSOR_FAULT_STARTED || trans == AlarmTransition::SENSOR_FAULT_CLEARED) {
            Serial.printf("[FAULT] Sensor Transition: %s\n", AlarmLogic::transitionToString(trans));
            snapshot.seq++;
            TelemetryEvent ev = makeEvent(snapshot, sr, trans, 0);
            eventQueue.push(ev);
        }
    } else {
        // Continuously update buzzer between sensor reads for accurate pitch/cadence
        updateBuzzer(now, snapshot.current_state);
    }

    // 3. Heartbeat Generator (Every 30s)
    if (now - last_heartbeat_ms >= HEARTBEAT_INTERVAL) {
        last_heartbeat_ms = now;
        SensorReading sr = readSensors();
        snapshot.seq++;
        TelemetryEvent hb = makeEvent(snapshot, sr, AlarmTransition::NONE, 0);
        eventQueue.push(hb);
    }

    // 4. Queue Dispatcher (Non-blocking transport)
    if (!eventQueue.empty() && (now - last_transport_attempt_ms >= transport_backoff_ms)) {
        last_transport_attempt_ms = now;
        TelemetryEvent frontEvent;
        if (eventQueue.peek(frontEvent)) {
            TransportResult res = postEvent(frontEvent);
            if (res == TransportResult::SENT || res == TransportResult::DUPLICATE_ACK) {
                // Successfully transmitted or duplicate recognized by server
                eventQueue.removeFront();
                transport_backoff_ms = 500; // Reset backoff
            } else if (res == TransportResult::PERMANENT_FAILURE) {
                // Drop permanently failing event to unblock queue
                Serial.printf("[TRANSPORT] Permanent failure for event seq %u. Dropping.\n", frontEvent.seq);
                eventQueue.removeFront();
                transport_backoff_ms = 1000;
            } else {
                // RETRYABLE_FAILURE: Exponential backoff with ceiling (max 30s)
                transport_backoff_ms = (transport_backoff_ms * 2);
                if (transport_backoff_ms > 30000) {
                    transport_backoff_ms = 30000;
                }
                Serial.printf("[TRANSPORT] Retryable failure. Backing off for %u ms.\n", transport_backoff_ms);
            }
        }
    }
}

#else
// Native entry point if compiled as firmware main
int main() {
    return 0;
}
#endif
