#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

// --- 1. Wi-Fi & VPS Endpoint ---
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// VPS IP đã triển khai và kiểm tra
#define SERVER_HOST "103.178.235.184"
#define SERVER_PORT 80
#define TELEMETRY_PATH "/api/v1/telemetry"

// Token xác thực Bearer cho thiết bị (khớp với VPS data/device_credentials.json)
#define DEVICE_ID "kitchen-01"
#define DEVICE_BEARER_TOKEN "secret-token-kitchen-01"

// --- 2. Sơ Đồ Chân GPIO ESP32 ---
#define PIN_BUZZER 19          // Còi báo động (qua mạch đệm transistor)
#define PIN_LED 2              // Đèn LED cảnh báo (hoặc LED tích hợp GPIO 2)
#define PIN_FLAME_AO 34        // Cảm biến ngọn lửa Analog (ADC1_CH6, 0-4095)
#define PIN_TEMP_DS18B20 4     // Cảm biến nhiệt độ DS18B20 (1-Wire, trở kéo 4.7k)

// Giao tiếp I2C cho Màn hình LCD 1602 & Cảm biến gia tốc MPU6050
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define LCD_I2C_ADDR 0x27      // Địa chỉ I2C phổ biến của LCD (0x27 hoặc 0x3F)

// --- 3. Tham Số Ngưỡng Theo Sơ Đồ Thuật Toán ---

// A. Rung động / Gia tốc (RĐ)
#define THRESHOLD_VIB_SAFE 0.2f        // RĐ < 0.2g: An Toàn
#define THRESHOLD_VIB_EARTHQUAKE 0.5f  // 0.2g <= RĐ <= 0.5g: Rung nhẹ; RĐ > 0.5g: Động Đất

// B. Ngọn lửa (NL - ADC 0-4095 - Đã tăng độ nhạy phát hiện lửa từ xa)
#define THRESHOLD_FLAME_FIRE 2800      // NL < 2800: Cảnh báo cháy (rất nhạy với lửa xa)
#define THRESHOLD_FLAME_SAFE 3600      // 2800 <= NL <= 3600: Cảnh báo sớm; NL > 3600: An Toàn

// C. Nhiệt độ (NĐ - °C)
#define THRESHOLD_TEMP_SAFE 38.0f      // NĐ < 38°C: An Toàn
#define THRESHOLD_TEMP_HIGH 45.0f      // 38°C <= NĐ <= 45°C: Nhiệt độ TB; NĐ > 45°C: Nhiệt độ cao

// --- 4. Thời Gian Lấy Mẫu & Gửi Tin ---
#define SENSOR_READ_INTERVAL_MS 100    // Đọc cảm biến mỗi 100ms
#define HEARTBEAT_INTERVAL_MS 30000    // Gửi nhịp tim định kỳ 30s khi bình thường

#endif // CONFIG_EXAMPLE_H
