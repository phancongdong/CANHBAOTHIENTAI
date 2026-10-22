#ifndef ALARM_LOGIC_H
#define ALARM_LOGIC_H

#include <cstdint>
#include <string>

// Mức chu kỳ Còi & LED theo sơ đồ
enum class CadenceMode {
    OFF,        // Còi: OFF, LED: OFF ("An Toàn")
    CADENCE_1HZ,// Còi: ON (1Hz), LED: ON (1Hz) - Cảnh báo mức TB / Sớm
    CADENCE_10HZ// Còi: ON (10Hz), LED: ON (10Hz) - Cảnh báo khẩn cấp (Cháy, Động Đất, Nhiệt Cao)
};

// Phân mức từng cảm biến theo sơ đồ
enum class VibrationLevel {
    SAFE,       // RD < 0.2g ("An Toàn")
    LIGHT,      // 0.2g <= RD <= 0.5g ("Rung nhẹ")
    EARTHQUAKE  // RD > 0.5g ("Động Đất")
};

enum class FlameLevel {
    SAFE,       // NL > 3200 ("An Toàn")
    EARLY_WARN, // 1600 <= NL <= 3200 ("Cảnh báo sớm")
    FIRE_ALARM  // NL < 1600 ("Cảnh báo cháy")
};

enum class TempLevel {
    SAFE,       // ND < 38°C ("An Toàn")
    MEDIUM,     // 38°C <= ND <= 45°C ("Nhiệt độ TB")
    HIGH        // ND > 45°C ("Nhiệt độ cao")
};

struct SensorReading {
    float vibration_g = 0.0f;     // Gia tốc/rung động RĐ (g)
    int flame_raw = 4095;         // Ngọn lửa NL (ADC: 0-4095)
    float temperature_c = 25.0f;  // Nhiệt độ NĐ (°C)
    bool connection_ok = true;    // Kiểm tra kết nối cảm biến
};

struct DisasterEvaluation {
    bool connection_ok = true;
    VibrationLevel vib_level = VibrationLevel::SAFE;
    FlameLevel flame_level = FlameLevel::SAFE;
    TempLevel temp_level = TempLevel::SAFE;

    CadenceMode cadence = CadenceMode::OFF;
    std::string lcd_line1; // Dòng 1 LCD: "An Toan", "Dong Dat", "Canh bao chay", v.v.
    std::string lcd_line2; // Dòng 2 LCD: Chi tiết thông số (RD, NL, ND)
    std::string alert_event; // Tên sự kiện: "heartbeat", "earthquake", "fire", "temp_high", v.v.
    bool is_emergency = false; // Mức 10Hz
    bool is_warning = false;   // Mức 1Hz
};

class DisasterAlarmLogic {
public:
    DisasterAlarmLogic() = default;

    // Đánh giá trạng thái theo đúng sơ đồ
    DisasterEvaluation evaluate(const SensorReading& reading);

    // Xác định chuyển trạng thái cảnh báo để gửi Telegram/Server
    bool hasStatusChanged(const DisasterEvaluation& current, std::string& out_event);

private:
    std::string m_last_event = "safe";
};

#endif // ALARM_LOGIC_H
