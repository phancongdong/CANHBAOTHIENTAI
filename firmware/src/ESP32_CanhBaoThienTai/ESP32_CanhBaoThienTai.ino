#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ======================================================================================
// 1. CẤU HÌNH CHÂN PHẦN CỨNG
// ======================================================================================
#define PIN_I2C_SDA         21  // SDA (MPU6050 / ADXL345 & LCD 1602)
#define PIN_I2C_SCL         22  // SCL (MPU6050 / ADXL345 & LCD 1602)
#define PIN_DHT             4   // Cảm biến nhiệt độ DHT11
#define PIN_FLAME_ANALOG    34  // Cảm biến ngọn lửa Analog AO
#define PIN_BUTTON_TEST     15  // Nút nhấn Test (Active LOW)
#define PIN_BUZZER          25  // Còi báo động (GPIO 25)
#define PIN_LED_ALERT       26  // Đèn LED cảnh báo (GPIO 26)

// ======================================================================================
// 2. CẤU HÌNH CÒI
// ======================================================================================
#define BUZZER_ACTIVE_LEVEL HIGH // HIGH: Kích mức cao | LOW: Kích mức thấp
#define BUZZER_IS_PASSIVE   0    // 0: Còi chủ động (Active) | 1: Còi thụ động (Passive)
#define BUZZER_PASSIVE_FREQ 2500 // Tần số cho còi thụ động (2.5kHz)

#define DHTTYPE             DHT11

// ======================================================================================
// 3. CẤU HÌNH WI-FI & TELEGRAM BOT
// ======================================================================================
#define ENABLE_TELEGRAM           1

// Tên & Mật khẩu Wi-Fi (Băng tần 2.4GHz):
const char* WIFI_SSID             = "A";  // Tên Wi-Fi của bạn
const char* WIFI_PASSWORD         = "chodehuy";   // Mật khẩu Wi-Fi của bạn

// Thông tin Bot Telegram:
const char* TELEGRAM_BOT_TOKEN    = "8622569426:AAGOlFk9Zfhxlj6d-WGSGb9C-GCdcYLM-9U";
const char* TELEGRAM_CHAT_ID      = "6130858384";

// Server VPS:
const char* VPS_URL               = "http://103.178.235.184/api/v1/telemetry";
const char* DEVICE_TOKEN          = "secret-token-kitchen-01";
const char* DEVICE_ID             = "kitchen-01";

// ======================================================================================
// 4. CÁC NGƯỠNG SO SÁNH THEO ĐÚNG LƯU ĐỒ
// ======================================================================================
// A. Rung động (RĐ - đơn vị: g):
const float RD_SAFE_MAX   = 0.20; // RĐ < 0.2g: An toàn
const float RD_WARN_MAX   = 0.70; // 0.2g - 0.5g: Rung nhẹ | RĐ > 0.5g: ĐỘNG ĐẤT NGUY HIỂM

// B. Nhiệt độ (NĐ - đơn vị: độ C):
const float ND_SAFE_MAX   = 38.0; // NĐ < 38°C: An toàn
const float ND_WARN_MAX   = 45.0; // 38°C - 45°C: Cảnh báo sớm | NĐ > 45°C: Nhiệt độ cao

// C. Ngọn lửa (NL - Analog ADC):
const int NL_FIRE_MAX     = 2800; // ADC < 2800: BÁO CHÁY NGAY (còi 10Hz)
const int NL_WARN_MAX     = 3600; // 2800 <= ADC <= 3600: Cảnh báo sớm (còi 1Hz)

// ======================================================================================
// 5. KHỞI TẠO ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC
// ======================================================================================
DHT dht(PIN_DHT, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

float val_RD = 0.0;
float val_ND = 25.0;
float val_DA = 60.0;
int   val_NL = 4095;

int level_RD = 0;
int level_ND = 0;
int level_NL = 0;
int system_max_level = 0;
int prev_system_level = 0;

bool is_mpu6050 = false;
bool is_adxl345 = false;
bool sensor_offline = false;

unsigned long timer_sample    = 0;
unsigned long timer_dht       = 0;
unsigned long timer_blink     = 0;
unsigned long timer_wifi_check= 0;
bool toggle_state = false;

String last_alert_event = "safe";
uint32_t packet_seq = 0;

// ======================================================================================
// 6. CƠ CHẾ ĐA NHÂN (DUAL CORE FREERTOS) - BÁO ĐỘNG NGAY 0ms KHÔNG CHỜ TELEGRAM!
// ======================================================================================
volatile bool alert_pending = false;
String pending_tele_msg = "";
String pending_event_name = "";
String pending_lcd_msg = "";
portMUX_TYPE alertMux = portMUX_INITIALIZER_UNLOCKED;

// ======================================================================================
// 7. ĐIỀU KHIỂN CÒI & ĐÈN
// ======================================================================================
void setBuzzer(bool enable) {
  if (enable) {
#if BUZZER_IS_PASSIVE
    tone(PIN_BUZZER, BUZZER_PASSIVE_FREQ);
#else
    digitalWrite(PIN_BUZZER, (BUZZER_ACTIVE_LEVEL == HIGH) ? HIGH : LOW);
#endif
  } else {
#if BUZZER_IS_PASSIVE
    noTone(PIN_BUZZER);
#else
    digitalWrite(PIN_BUZZER, (BUZZER_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
#endif
  }
}

void beepBuzzer(int count, int on_ms, int off_ms) {
  for (int i = 0; i < count; i++) {
    setBuzzer(true);
    digitalWrite(PIN_LED_ALERT, HIGH);
    delay(on_ms);
    setBuzzer(false);
    digitalWrite(PIN_LED_ALERT, LOW);
    if (i < count - 1) delay(off_ms);
  }
}

// ======================================================================================
// 8. ĐỌC CẢM BIẾN LỬA & GIA TỐC
// ======================================================================================
int readFlameFiltered() {
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_FLAME_ANALOG);
    delayMicroseconds(40);
  }
  return (int)(sum / 8);
}

bool initAccelerometer() {
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() == 0) {
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission();
    is_mpu6050 = true;
    Serial.println(F("[SENSOR] Da ket noi MPU-6050 (GY-521)!"));
    return true;
  }

  Wire.beginTransmission(0x53);
  if (Wire.endTransmission() == 0) {
    Wire.beginTransmission(0x53);
    Wire.write(0x2D);
    Wire.write(0x08);
    Wire.endTransmission();
    is_adxl345 = true;
    Serial.println(F("[SENSOR] Da ket noi ADXL-345!"));
    return true;
  }
  return false;
}

bool isAccelOnline() {
  if (is_mpu6050) {
    Wire.beginTransmission(0x68);
    return (Wire.endTransmission() == 0);
  } else if (is_adxl345) {
    Wire.beginTransmission(0x53);
    return (Wire.endTransmission() == 0);
  }
  return false;
}

float readVibrationG() {
  if (is_mpu6050) {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return 0.0;
    if (Wire.requestFrom((uint8_t)0x68, (size_t)6) == 6) {
      int16_t ax = (Wire.read() << 8) | Wire.read();
      int16_t ay = (Wire.read() << 8) | Wire.read();
      int16_t az = (Wire.read() << 8) | Wire.read();
      float gx = ax / 16384.0f;
      float gy = ay / 16384.0f;
      float gz = az / 16384.0f;
      float total = sqrt(gx * gx + gy * gy + gz * gz);
      return fabs(total - 1.0f);
    }
  } else if (is_adxl345) {
    Wire.beginTransmission(0x53);
    Wire.write(0x32);
    if (Wire.endTransmission(false) != 0) return 0.0;
    if (Wire.requestFrom((uint8_t)0x53, (size_t)6) == 6) {
      int16_t ax = Wire.read() | (Wire.read() << 8);
      int16_t ay = Wire.read() | (Wire.read() << 8);
      int16_t az = Wire.read() | (Wire.read() << 8);
      float gx = ax * 0.0039f;
      float gy = ay * 0.0039f;
      float gz = az * 0.0039f;
      float total = sqrt(gx * gx + gy * gy + gz * gz);
      return fabs(total - 1.0f);
    }
  }
  return 0.0;
}

// ======================================================================================
// 9. CÁC HÀM GỬI MẠNG (CHẠY NGẦM TRÊN CORE 0)
// ======================================================================================
String escapeJson(const String& input) {
  String out = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') continue;
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

void sendDirectToTelegram(const String& message) {
#if !ENABLE_TELEGRAM
  return;
#endif
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(3500);

  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) + "\",";
    jsonPayload += "\"text\":\"" + escapeJson(message) + "\",";
    jsonPayload += "\"parse_mode\":\"HTML\"";
    jsonPayload += "}";

    https.POST(jsonPayload);
    https.end();
  }
}

void sendTelemetryToVPS(const String& eventName, const String& lcdMsg) {
#if !ENABLE_TELEGRAM
  return;
#endif
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(VPS_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);
  http.setTimeout(2500);

  packet_seq++;
  String payload = "{";
  payload += "\"schema_version\":1,";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"boot_id\":\"esp32-hw-01\",";
  payload += "\"seq\":" + String(packet_seq) + ",";
  payload += "\"uptime_ms\":" + String(millis()) + ",";
  payload += "\"sample_age_ms\":0,";
  payload += "\"vibration_g\":" + String(val_RD, 2) + ",";
  payload += "\"flame_raw\":" + String(val_NL) + ",";
  payload += "\"temperature_c\":" + String(val_ND, 1) + ",";
  payload += "\"flame_detected\":" + String(val_NL < NL_FIRE_MAX ? "true" : "false") + ",";
  payload += "\"sensor_fault\":" + String(sensor_offline ? "true" : "false") + ",";
  payload += "\"alarm_state\":\"" + String(system_max_level == 2 ? "ALARM" : (system_max_level == 1 ? "SUSPECT" : "NORMAL")) + "\",";
  payload += "\"alarm_latched\":false,";
  payload += "\"event\":\"" + eventName + "\",";
  payload += "\"lcd_message\":\"" + lcdMsg + "\"";
  payload += "}";

  http.POST(payload);
  http.end();
}

// TÁC VỤ CHẠY NGẦM TRÊN CORE 0:
void backgroundNetworkTask(void* pvParameters) {
  for (;;) {
    if (alert_pending) {
      String msgToSend;
      String evtToSend;
      String lcdToSend;

      portENTER_CRITICAL(&alertMux);
      msgToSend = pending_tele_msg;
      evtToSend = pending_event_name;
      lcdToSend = pending_lcd_msg;
      alert_pending = false;
      portEXIT_CRITICAL(&alertMux);

      sendDirectToTelegram(msgToSend);
      sendTelemetryToVPS(evtToSend, lcdToSend);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void triggerAlert(const String& eventName, const String& lcdMsg, const String& teleMsg) {
  if (eventName == last_alert_event) return;
  last_alert_event = eventName;

  Serial.printf("\n>>> [CANH BAO TUC THI] Event: %s\n", eventName.c_str());

  portENTER_CRITICAL(&alertMux);
  pending_event_name = eventName;
  pending_lcd_msg    = lcdMsg;
  pending_tele_msg   = teleMsg;
  alert_pending      = true;
  portEXIT_CRITICAL(&alertMux);
}

void connectWiFiAsync() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ======================================================================================
// 10. SETUP
// ======================================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_BUTTON_TEST, INPUT_PULLUP);
  pinMode(PIN_FLAME_ANALOG, INPUT);

  setBuzzer(false);
  digitalWrite(PIN_LED_ALERT, LOW);

  beepBuzzer(2, 80, 60);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CANH BAO T.TAI  ");
  lcd.setCursor(0, 1);
  lcd.print("KHOI DONG...    ");

  initAccelerometer();
  dht.begin();

  // Khởi tạo tác vụ gửi mạng ngầm trên CORE 0
  xTaskCreatePinnedToCore(
      backgroundNetworkTask,
      "NetWorker",
      10240,
      NULL,
      1,
      NULL,
      0
  );

  connectWiFiAsync();

  unsigned long startW = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startW < 4000) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    triggerAlert("recovery", "He Thong San Sang",
                 "✅ <b>[HỆ THỐNG ĐÃ SẴN SÀNG]</b>\n"
                 "📍 <b>Thiết bị:</b> " + String(DEVICE_ID) + "\n"
                 "⚡ Cơ chế báo động tức thì 0ms đã kích hoạt!");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG READY  ");
  lcd.setCursor(0, 1);
  lcd.print("GIAM SAT 24/7   ");
  delay(600);
  lcd.clear();
}

// ======================================================================================
// 11. LOOP (CHẠY TRÊN CORE 1 - BÁO ĐỘNG NGAY 0 MILIGIÂY)
// ======================================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. NÚT TEST PHẦN CỨNG (GPIO 15)
  if (digitalRead(PIN_BUTTON_TEST) == LOW) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TEST COI & LED  ");
    lcd.setCursor(0, 1);
    lcd.print("DANG KEU BEEP!  ");
    beepBuzzer(3, 120, 80);

    triggerAlert("button_test", "Nhan Nut Test",
                 "🧪 <b>[KIỂM TRA HỆ THỐNG - NÚT TEST]</b>\n"
                 "📍 <b>Thiết bị:</b> " + String(DEVICE_ID) + "\n"
                 "🔔 Còi & Đèn báo động tại chỗ hoạt động tốt!");
    delay(200);
  }

  // 2. ĐỌC NHIỆT ĐỘ ĐỊNH KỲ (Mỗi 1.5 giây để tránh lỗi DHT11)
  if (currentMillis - timer_dht >= 1500) {
    timer_dht = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) val_ND = t;
    if (!isnan(h)) val_DA = h;
  }

  // 3. CHU KỲ QUÉT CẢM BIẾN NHANH (MỖI 50ms ĐỂ BẮT RUNG LẮC CỰC NHẠY)
  if (currentMillis - timer_sample >= 50) {
    timer_sample = currentMillis;

    // --- KIỂM TRA CẢM BIẾN GIA TỐC CÒN ONLINE KHÔNG ---
    bool accel_ok = isAccelOnline();
    if (!accel_ok) {
      sensor_offline = true;
    } else {
      sensor_offline = false;
      val_RD = readVibrationG();
    }
    val_NL = readFlameFiltered();

    // ====================================================================
    // TRƯỜNG HỢP 1: CẢM BIẾN BỊ OFFLINE / TUỘT DÂY -> BÁO ĐỘNG NGAY LẬP TỨC!
    // ====================================================================
    if (sensor_offline) {
      system_max_level = 2; // Ép mức báo động khẩn cấp!

      // BẬT CÒI VÀ ĐÈN TỨC THÌ (0ms trễ)
      setBuzzer(true);
      digitalWrite(PIN_LED_ALERT, HIGH);

      lcd.setCursor(0, 0);
      lcd.print("! LOI PHAN CUNG!");
      lcd.setCursor(0, 1);
      lcd.print("CAM BIEN OFFLINE");

      triggerAlert("sensor_fault", "CAM BIEN OFFLINE",
                   "🚨⚠️ <b>[CẢNH BÁO LỖI: CẢM BIẾN OFFLINE]</b>\n"
                   "📍 <b>Thiết bị:</b> " + String(DEVICE_ID) + "\n"
                   "❌ Mất tín hiệu I2C cảm biến gia tốc MPU-6050/ADXL345!\n"
                   "📢 <b>Còi báo động:</b> Đang hú tại chỗ!\n"
                   "⚠️ <i>Kiểm tra lại dây nối SDA (21) / SCL (22) ngay!</i>");
      return;
    }

    // ====================================================================
    // TRƯỜNG HỢP 2: HOẠT ĐỘNG BÌNH THƯỜNG - SO KHỚP CÁC NGƯỠNG LƯU ĐỒ
    // ====================================================================
    // A. Nhánh Rung Động (Động Đất)
    if (val_RD < RD_SAFE_MAX) {
      level_RD = 0; // An toàn
    } else if (val_RD <= RD_WARN_MAX) {
      level_RD = 1; // 0.2g - 0.5g: Rung nhẹ
    } else {
      level_RD = 2; // > 0.5g: ĐỘNG ĐẤT NGUY HIỂM!
    }

    // B. Nhánh Nhiệt Độ
    if (val_ND > ND_WARN_MAX) {
      level_ND = 2; // > 45°C
    } else if (val_ND >= ND_SAFE_MAX) {
      level_ND = 1; // 38°C - 45°C
    } else {
      level_ND = 0;
    }

    // C. Nhánh Ngọn Lửa
    if (val_NL < NL_FIRE_MAX) {
      level_NL = 2; // Báo Cháy
    } else if (val_NL <= NL_WARN_MAX) {
      level_NL = 1; // Cảnh báo sớm
    } else {
      level_NL = 0;
    }

    system_max_level = max(level_RD, max(level_ND, level_NL));

    // ====================================================================
    // [QUAN TRỌNG] BẬT BÁO ĐỘNG TỨC THÌ (ZERO DELAY) KHI VỪA PHÁT HIỆN
    // ====================================================================
    if (system_max_level == 2 && prev_system_level != 2) {
      // Động đất hoặc cháy: Bật còi & đèn NGAY Ở MILIGIÂY ĐẦU TIÊN!
      setBuzzer(true);
      digitalWrite(PIN_LED_ALERT, HIGH);
      toggle_state = true;
      timer_blink = currentMillis;
    }
    prev_system_level = system_max_level;

    // --- HIỂN THỊ MÀN HÌNH LCD 1602 (CHUẨN 16 KÝ TỰ, ƯU TIÊN HIỂN THỊ CẢM BIẾN LỬA) ---
    char line0[17];
    char line1[17];
    String currentEvent = "safe";
    String teleMsg = "";

    if (system_max_level == 2) {
      if (level_NL == 2) {
        snprintf(line0, sizeof(line0), "LUA: %-4d [CHAY]", val_NL);
        snprintf(line1, sizeof(line1), "!CANH BAO CHAY! ");
        currentEvent = "fire_alarm";
        teleMsg = "🔥🚨 <b>[CẢNH BÁO CHÁY KHẨN CẤP]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "🔥 <b>Ngọn lửa:</b> Phát hiện! (ADC: " + String(val_NL) + ")\n"
                  "🌡 <b>Nhiệt độ:</b> " + String(val_ND, 1) + " °C\n"
                  "📢 <b>Còi & LED:</b> Đang hú 10Hz dồn dập tại chỗ!";
      } else if (level_RD == 2) {
        snprintf(line0, sizeof(line0), "RUNG CHAN: %0.2fg", val_RD);
        snprintf(line1, sizeof(line1), "! DONG DAT !    ");
        currentEvent = "earthquake";
        teleMsg = "🚨🚨 <b>[CẢNH BÁO ĐỘNG ĐẤT NGUY HIỂM]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "⚠️ <b>Rung chấn:</b> " + String(val_RD, 2) + "g (> 0.5g)!\n"
                  "📢 <b>Còi & LED:</b> Đang hú 10Hz khẩn cấp tại chỗ!\n"
                  "⚠️ <i>Hãy tìm nơi trú ẩn an toàn ngay lập tức!</i>";
      } else {
        snprintf(line0, sizeof(line0), "NHIET DO: %0.1fC ", val_ND);
        snprintf(line1, sizeof(line1), "!NHIET DO CAO!  ");
        currentEvent = "temp_high";
        teleMsg = "🌡🚨 <b>[CẢNH BÁO NHIỆT ĐỘ QUÁ CAO]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "🌡 <b>Nhiệt độ:</b> " + String(val_ND, 1) + " °C (> 45°C)!\n"
                  "📢 <b>Còi & LED:</b> Đang hú 10Hz tại chỗ!";
      }
    } else if (system_max_level == 1) {
      if (level_NL == 1) {
        snprintf(line0, sizeof(line0), "LUA: %-4d [CANH]", val_NL);
        snprintf(line1, sizeof(line1), "CANH BAO LUA!   ");
        currentEvent = "flame_early";
        teleMsg = "⚠️ <b>[CẢNH BÁO SỚM NGỌN LỬA]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "ℹ️ Bức xạ lửa khả nghi (ADC: " + String(val_NL) + ")\n"
                  "📢 Còi nhấp nháy 1Hz.";
      } else if (level_RD == 1) {
        snprintf(line0, sizeof(line0), "RUNG CHAN: %0.2fg", val_RD);
        snprintf(line1, sizeof(line1), "CANH BAO RUNG   ");
        currentEvent = "vibration_light";
        teleMsg = "⚠️ <b>[CẢNH BÁO RUNG NHẸ]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "ℹ️ Rung chấn: " + String(val_RD, 2) + "g (0.2g - 0.5g)\n"
                  "📢 Còi nhấp nháy 1Hz cảnh báo.";
      } else {
        snprintf(line0, sizeof(line0), "NHIET DO: %0.1fC ", val_ND);
        snprintf(line1, sizeof(line1), "CANH BAO NHIET  ");
        currentEvent = "temp_medium";
        teleMsg = "⚠️ <b>[CẢNH BÁO NHIỆT ĐỘ TRUNG BÌNH]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "ℹ️ Nhiệt độ: " + String(val_ND, 1) + " °C (38°C - 45°C).";
      }
    } else {
      // TRẠNG THÁI BÌNH THƯỜNG / AN TOÀN:
      // Hàng 0: Hiển thị giá trị cảm biến Lửa (ADC) rất rõ ràng kèm trạng thái [OK]
      snprintf(line0, sizeof(line0), "LUA: %-4d   [OK] ", val_NL);
      // Hàng 1: Hiển thị độ rung chấn (g) và nhiệt độ (°C)
      snprintf(line1, sizeof(line1), "R:%0.1fg  T:%2.0fC   ", val_RD, val_ND);
      currentEvent = "safe";
      teleMsg = "✅ <b>[HỆ THỐNG AN TOÀN TRỞ LẠI]</b>\n"
                "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                "🔇 Còi & Đèn LED: ĐÃ TẮT.\n"
                "ℹ️ Các chỉ số cảm biến đã về mức an toàn.";
    }

    lcd.setCursor(0, 0);
    lcd.print(line0);
    lcd.setCursor(0, 1);
    lcd.print(line1);

    // Kích hoạt gửi Telegram ngầm trên Core 0 (Không làm chậm còi)
    triggerAlert(currentEvent, String(line1), teleMsg);
  }

  // 4. ĐIỀU KHIỂN CÒI & LED THEO CHU KỲ (NON-BLOCKING)
  if (system_max_level == 0 && !sensor_offline) {
    setBuzzer(false);
    digitalWrite(PIN_LED_ALERT, LOW);
  } else if (system_max_level == 1) {
    if (currentMillis - timer_blink >= 500) {
      timer_blink = currentMillis;
      toggle_state = !toggle_state;
      setBuzzer(toggle_state);
      digitalWrite(PIN_LED_ALERT, toggle_state ? HIGH : LOW);
    }
  } else if (system_max_level == 2 || sensor_offline) {
    if (currentMillis - timer_blink >= 50) {
      timer_blink = currentMillis;
      toggle_state = !toggle_state;
      setBuzzer(toggle_state);
      digitalWrite(PIN_LED_ALERT, toggle_state ? HIGH : LOW);
    }
  }

  // 5. TỰ ĐỘNG KẾT NỐI LẠI WI-FI (Không gây treo còi)
  if (currentMillis - timer_wifi_check >= 10000) {
    timer_wifi_check = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
  }
}