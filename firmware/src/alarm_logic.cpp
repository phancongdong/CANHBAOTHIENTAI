#include "alarm_logic.h"
#include <cstdio>

DisasterEvaluation DisasterAlarmLogic::evaluate(const SensorReading& reading) {
    DisasterEvaluation eval;
    eval.connection_ok = reading.connection_ok;

    // 1. Kiểm tra kết nối cảm biến
    if (!reading.connection_ok) {
        eval.cadence = CadenceMode::CADENCE_1HZ;
        eval.lcd_line1 = "Thong Bao Loi";
        eval.lcd_line2 = "Mat ket noi CB!";
        eval.alert_event = "sensor_fault";
        return eval;
    }

    // 2. Nhánh Cảm biến Rung Động (RĐ)
    if (reading.vibration_g > 0.5f) {
        eval.vib_level = VibrationLevel::EARTHQUAKE;
    } else if (reading.vibration_g >= 0.2f) {
        eval.vib_level = VibrationLevel::LIGHT;
    } else {
        eval.vib_level = VibrationLevel::SAFE;
    }

    // 3. Nhánh Cảm biến Ngọn Lửa (NL) - Tăng độ nhạy phát hiện lửa xa
    if (reading.flame_raw < 2800) {
        eval.flame_level = FlameLevel::FIRE_ALARM;
    } else if (reading.flame_raw <= 3600) {
        eval.flame_level = FlameLevel::EARLY_WARN;
    } else {
        eval.flame_level = FlameLevel::SAFE;
    }

    // 4. Nhánh Cảm biến Nhiệt Độ (NĐ)
    if (reading.temperature_c > 45.0f) {
        eval.temp_level = TempLevel::HIGH;
    } else if (reading.temperature_c >= 38.0f) {
        eval.temp_level = TempLevel::MEDIUM;
    } else {
        eval.temp_level = TempLevel::SAFE;
    }

    // 5. Tổng hợp theo mức độ ưu tiên theo đúng sơ đồ
    // Ưu tiên 1: Cảnh báo khẩn cấp (Còi: ON 10Hz, LED: ON 10Hz)
    if (eval.vib_level == VibrationLevel::EARTHQUAKE) {
        eval.cadence = CadenceMode::CADENCE_10HZ;
        eval.lcd_line1 = "Dong Dat";
        eval.alert_event = "earthquake";
        eval.is_emergency = true;
    } else if (eval.flame_level == FlameLevel::FIRE_ALARM) {
        eval.cadence = CadenceMode::CADENCE_10HZ;
        eval.lcd_line1 = "Canh bao chay";
        eval.alert_event = "fire_alarm";
        eval.is_emergency = true;
    } else if (eval.temp_level == TempLevel::HIGH) {
        eval.cadence = CadenceMode::CADENCE_10HZ;
        eval.lcd_line1 = "Nhiet do cao";
        eval.alert_event = "temp_high";
        eval.is_emergency = true;
    }
    // Ưu tiên 2: Cảnh báo mức trung bình / sớm (Còi: ON 1Hz, LED: ON 1Hz)
    else if (eval.vib_level == VibrationLevel::LIGHT) {
        eval.cadence = CadenceMode::CADENCE_1HZ;
        eval.lcd_line1 = "Rung nhe";
        eval.alert_event = "vibration_light";
        eval.is_warning = true;
    } else if (eval.flame_level == FlameLevel::EARLY_WARN) {
        eval.cadence = CadenceMode::CADENCE_1HZ;
        eval.lcd_line1 = "Canh bao som";
        eval.alert_event = "flame_early";
        eval.is_warning = true;
    } else if (eval.temp_level == TempLevel::MEDIUM) {
        eval.cadence = CadenceMode::CADENCE_1HZ;
        eval.lcd_line1 = "Nhiet do TB";
        eval.alert_event = "temp_medium";
        eval.is_warning = true;
    }
    // Ưu tiên 3: An toàn (Còi: OFF, LED: OFF)
    else {
        eval.cadence = CadenceMode::OFF;
        eval.lcd_line1 = "An Toan";
        eval.alert_event = "safe";
    }

    // Định dạng Dòng 2 hiển thị LCD: "RD:0.1g L:3500 T:28"
    char buf[20];
    snprintf(buf, sizeof(buf), "R:%.2fg L:%d T:%.0f",
             reading.vibration_g, reading.flame_raw, reading.temperature_c);
    eval.lcd_line2 = buf;

    return eval;
}

bool DisasterAlarmLogic::hasStatusChanged(const DisasterEvaluation& current, std::string& out_event) {
    if (current.alert_event != m_last_event) {
        m_last_event = current.alert_event;
        out_event = current.alert_event;
        return true;
    }
    return false;
}
