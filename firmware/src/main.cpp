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
static DisasterAlarmLogic alarmLogic;
static EventQueue eventQueue(50);
static DeviceSnapshot snapshot;

static uint32_t last_sensor_read_ms = 0;
static uint32_t last_heartbeat_ms = 0;
static uint32_t last_transport_attempt_ms = 0;
static uint32_t transport_backoff_ms = 1000;

static CadenceMode current_cadence = CadenceMode::OFF;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=============================================");
    Serial.println("  HE THONG CANH BAO THIEN TAI & BAO CHAY ESP32");
    Serial.println("=============================================");

    // 1. Khoi tao NVS de luu boot_id
    prefs.begin("disaster_sys", false);
    uint32_t boot_count = prefs.getUInt("boot_count", 0) + 1;
    prefs.putUInt("boot_count", boot_count);

    char boot_buf[32];
    snprintf(boot_buf, sizeof(boot_buf), "boot-%06u", boot_count);
    snapshot.device_id = DEVICE_ID;
    snapshot.boot_id = boot_buf;
    snapshot.seq = 0;

    // 2. Khoi tao thiet bi dau ra (LCD, Coi, LED)
    initLcd(LCD_I2C_ADDR, PIN_I2C_SDA, PIN_I2C_SCL);
    initAlertOutputs(PIN_BUZZER, PIN_LED);
    testBuzzer(2, 100, 80); // Test tieng bip coi khoi dong

    // 3. Khoi tao cac cam bien (Gia toc MPU6050, Lua Analog, Nhiet DS18B20)
    initSensors(PIN_FLAME_AO, PIN_TEMP_DS18B20, PIN_I2C_SDA, PIN_I2C_SCL);

    // 4. Khoi tao Transport ket noi VPS
    initTransport(SERVER_HOST, SERVER_PORT, DEVICE_BEARER_TOKEN);

    // 5. Ket noi Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[WIFI] Dang ket noi toi: %s\n", WIFI_SSID);

    updateLcd("He Thong San Sang", "Khoi Dong Xong!");
    delay(1000);
}

void loop() {
    uint32_t now = millis();
    snapshot.uptime_ms = now;

    // 1. Doc cam bien va danh gia trang thai dinh ky (100ms)
    if (now - last_sensor_read_ms >= SENSOR_READ_INTERVAL_MS) {
        last_sensor_read_ms = now;

        SensorReading reading = readSensors();
        DisasterEvaluation eval = alarmLogic.evaluate(reading);

        // Cap nhat che do Coi & LED
        current_cadence = eval.cadence;

        // Cap nhat man hinh LCD theo so do
        updateLcd(eval.lcd_line1, eval.lcd_line2);

        // Kiem tra chuyen trang thai de gui canh bao ve VPS & Telegram
        std::string changed_event;
        if (alarmLogic.hasStatusChanged(eval, changed_event)) {
            Serial.printf("[EVENT] Trang thai moi: %s (LCD: %s)\n",
                          changed_event.c_str(), eval.lcd_line1.c_str());

            snapshot.seq++;
            snapshot.alert_event = changed_event;
            snapshot.lcd_line1 = eval.lcd_line1;
            snapshot.is_emergency = eval.is_emergency;

            TelemetryEvent ev = makeEvent(snapshot, reading, eval, 0);
            eventQueue.push(ev);
        }
    }

    // 2. Dieu khien chu ky nhap nhay Coi & LED lien tuc theo thoi gian thuc
    updateAlertOutputs(now, current_cadence);

    // 3. Gui nhip tim dinh ky ve VPS (30 giay) khi khong co su co
    if (now - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
        last_heartbeat_ms = now;

        SensorReading reading = readSensors();
        DisasterEvaluation eval = alarmLogic.evaluate(reading);

        snapshot.seq++;
        snapshot.alert_event = "heartbeat";
        snapshot.lcd_line1 = eval.lcd_line1;
        snapshot.is_emergency = eval.is_emergency;

        TelemetryEvent hb = makeEvent(snapshot, reading, eval, 0);
        eventQueue.push(hb);
    }

    // 4. Gui cac ban tin trong hang doi len VPS qua HTTP
    if (!eventQueue.empty() && (now - last_transport_attempt_ms >= transport_backoff_ms)) {
        last_transport_attempt_ms = now;
        TelemetryEvent frontEvent;
        if (eventQueue.peek(frontEvent)) {
            TransportResult res = postEvent(frontEvent);
            if (res == TransportResult::SENT || res == TransportResult::DUPLICATE_ACK) {
                eventQueue.removeFront();
                transport_backoff_ms = 500;
            } else if (res == TransportResult::PERMANENT_FAILURE) {
                Serial.printf("[TRANSPORT] Permanent failure seq %u. Bo qua.\n", frontEvent.seq);
                eventQueue.removeFront();
                transport_backoff_ms = 1000;
            } else {
                transport_backoff_ms = std::min(transport_backoff_ms * 2, 30000u);
                Serial.printf("[TRANSPORT] Thu lai sau %u ms...\n", transport_backoff_ms);
            }
        }
    }
}

#else
int main() {
    return 0;
}
#endif
