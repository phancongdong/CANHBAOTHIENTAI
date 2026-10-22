#include "drivers.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static int g_buzzer_pin = 19;
static int g_led_pin = 2;
static uint32_t g_last_toggle_ms = 0;
static bool g_output_state = false;

static LiquidCrystal_I2C* g_lcd = nullptr;
static std::string g_current_lcd_line1 = "";
static std::string g_current_lcd_line2 = "";

void initAlertOutputs(int buzzer_pin, int led_pin) {
    g_buzzer_pin = buzzer_pin;
    g_led_pin = led_pin;

    pinMode(g_buzzer_pin, OUTPUT);
    pinMode(g_led_pin, OUTPUT);
    digitalWrite(g_buzzer_pin, LOW);
    digitalWrite(g_led_pin, LOW);
    g_output_state = false;
}

void setAlertOutputsDirect(bool buzzer_on, bool led_on) {
    digitalWrite(g_buzzer_pin, buzzer_on ? HIGH : LOW);
    digitalWrite(g_led_pin, led_on ? HIGH : LOW);
    g_output_state = buzzer_on || led_on;
}

void testBuzzer(int beeps, int on_ms, int off_ms) {
    for (int i = 0; i < beeps; i++) {
        digitalWrite(g_buzzer_pin, HIGH);
        digitalWrite(g_led_pin, HIGH);
        delay(on_ms);
        digitalWrite(g_buzzer_pin, LOW);
        digitalWrite(g_led_pin, LOW);
        if (i < beeps - 1) delay(off_ms);
    }
}

void updateAlertOutputs(uint32_t now_ms, CadenceMode cadence) {
    switch (cadence) {
        case CadenceMode::CADENCE_10HZ: {
            // Chu kỳ 10Hz: 50ms BẬT / 50ms TẮT
            if (now_ms - g_last_toggle_ms >= 50) {
                g_output_state = !g_output_state;
                digitalWrite(g_buzzer_pin, g_output_state ? HIGH : LOW);
                digitalWrite(g_led_pin, g_output_state ? HIGH : LOW);
                g_last_toggle_ms = now_ms;
            }
            break;
        }

        case CadenceMode::CADENCE_1HZ: {
            // Chu kỳ 1Hz: 500ms BẬT / 500ms TẮT
            if (now_ms - g_last_toggle_ms >= 500) {
                g_output_state = !g_output_state;
                digitalWrite(g_buzzer_pin, g_output_state ? HIGH : LOW);
                digitalWrite(g_led_pin, g_output_state ? HIGH : LOW);
                g_last_toggle_ms = now_ms;
            }
            break;
        }

        case CadenceMode::OFF:
        default:
            if (g_output_state) {
                digitalWrite(g_buzzer_pin, LOW);
                digitalWrite(g_led_pin, LOW);
                g_output_state = false;
            }
            break;
    }
}

void initLcd(uint8_t i2c_addr, int sda_pin, int scl_pin) {
    g_lcd = new LiquidCrystal_I2C(i2c_addr, 16, 2);
    g_lcd->init();
    g_lcd->backlight();
    g_lcd->clear();
    g_lcd->setCursor(0, 0);
    g_lcd->print("He Thong Canh Bao");
    g_lcd->setCursor(0, 1);
    g_lcd->print("Dang Khoi Tao...");
}

void updateLcd(const std::string& line1, const std::string& line2) {
    if (!g_lcd) return;

    if (line1 != g_current_lcd_line1 || line2 != g_current_lcd_line2) {
        g_current_lcd_line1 = line1;
        g_current_lcd_line2 = line2;

        g_lcd->clear();
        g_lcd->setCursor(0, 0);
        g_lcd->print(line1.c_str());

        g_lcd->setCursor(0, 1);
        g_lcd->print(line2.c_str());
    }
}

#else
// Native stubs
void initAlertOutputs(int buzzer_pin, int led_pin) {}
void setAlertOutputsDirect(bool buzzer_on, bool led_on) {}
void updateAlertOutputs(uint32_t now_ms, CadenceMode cadence) {}
void testBuzzer(int beeps, int on_ms, int off_ms) {}
void initLcd(uint8_t i2c_addr, int sda_pin, int scl_pin) {}
void updateLcd(const std::string& line1, const std::string& line2) {}
#endif
