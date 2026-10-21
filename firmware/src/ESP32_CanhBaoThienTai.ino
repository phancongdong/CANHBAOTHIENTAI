#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ======================================================================================
// 1. CẤU HÌNH CHÂN PHẦN CỨNG (KHỚP HOÀN TOÀN THEO MẠCH CỦA BẠN)
// ======================================================================================
#define PIN_I2C_SDA         21  // SDA (Dùng chung cho MPU6050/ADXL345 và LCD 1602)
#define PIN_I2C_SCL         22  // SCL (Dùng chung cho MPU6050/ADXL345 và LCD 1602)
#define PIN_DHT             4   // Data cảm biến nhiệt độ DHT11
#define PIN_FLAME_ANALOG    34  // Analog (AO) cảm biến ngọn lửa (ADC1_CH6)
#define PIN_BUTTON_TEST     15  // Nút nhấn Test / Reset (Active LOW)
#define PIN_BUZZER          25  // Còi báo động (Kiểm tra đúng chân GPIO 25 hay 19 trên mạch của bạn)
#define PIN_LED_ALERT       26  // Đèn LED cảnh báo

// ======================================================================================
// [QUAN TRỌNG] CẤU HÌNH LOẠI CÒI & MỨC KÍCH (GIẢI PHÁP KHI CÒI KHÔNG KÊU)
// ======================================================================================
// A. MỨC KÍCH HOẠT (TRIGGER LEVEL):
//    - HIGH: Kích mức cao (3.3V/5V -> KÊU, 0V -> TẮT) [Mặc định đa số mạch đệm NPN S8050]
//    - LOW : Kích mức thấp (0V -> KÊU, 3.3V -> TẮT) [Nhiều module còi 3 chân dùng transistor PNP kích LOW]
#define BUZZER_ACTIVE_LEVEL HIGH

// B. LOẠI CÒI (BUZZER TYPE):
//    - 0: Còi chủ động (Active Buzzer - có mạch dao động sẵn, đít bọc keo đen, cấp DC là kêu)
//    - 1: Còi thụ động (Passive Buzzer - mạch hở xanh lá, PHẢI phát xung PWM mới kêu)
#define BUZZER_IS_PASSIVE   0
#define BUZZER_PASSIVE_FREQ 2500 // Tần số cộng hưởng 2.5kHz cho còi thụ động

#define DHTTYPE             DHT11

// ======================================================================================
// 2. CẤU HÌNH WI-FI & TELEGRAM BOT (GỬI THẲNG VÀO ĐIỆN THOẠI CỦA BẠN)
// ======================================================================================
// [CÔNG TẮC BẬT GỬI TELEGRAM]:
//   - 1: BẬT gửi tín hiệu lên Telegram (Báo cháy, động đất, nhiệt độ cao, nút test)
//   - 0: TẮT gửi tín hiệu Telegram
#define ENABLE_TELEGRAM           1

// TÀI KHOẢN WI-FI (BẮT BUỘC ĐỔI THÀNH WI-FI NHÀ BẠN - BĂNG TẦN 2.4GHz):
const char* WIFI_SSID             = "Adudulubu";  // ĐỔI thành tên Wi-Fi nhà bạn
const char* WIFI_PASSWORD         = "chodehuy";   // ĐỔI thành mật khẩu Wi-Fi nhà bạn

// THÔNG TIN BOT TELEGRAM TRỰC TIẾP (BẢN TIN ĐI THẲNG TỪ ESP32 VÀO TELEGRAM):
const char* TELEGRAM_BOT_TOKEN    = "8622569426:AAGOlFk9Zfhxlj6d-WGSGb9C-GCdcYLM-9U";
const char* TELEGRAM_CHAT_ID      = "6130858384";

// Server VPS dự phòng (Lưu trữ và đồng bộ telemetry):
const char* VPS_URL               = "http://103.178.235.184/api/v1/telemetry";
const char* DEVICE_TOKEN          = "secret-token-kitchen-01";
const char* DEVICE_ID             = "kitchen-01";

// ======================================================================================
// 3. CÁC NGƯỠNG SO SÁNH THEO ĐÚNG LƯU ĐỒ KHỐI
// ======================================================================================
// A. Rung động (RĐ - đơn vị: g):
const float RD_SAFE_MAX   = 0.20; // RĐ < 0.2g: An toàn
const float RD_WARN_MAX   = 0.50; // 0.2g <= RĐ <= 0.5g: Rung nhẹ | RĐ > 0.5g: Động đất

// B. Nhiệt độ (NĐ - đơn vị: độ C):
const float ND_SAFE_MAX   = 38.0; // NĐ < 38°C: An toàn
const float ND_WARN_MAX   = 45.0; // 38°C <= NĐ <= 45°C: Nhiệt độ TB | NĐ > 45°C: Nhiệt độ cao

// C. Ngọn lửa (NL - Analog ADC 0-4095):
// --------------------------------------------------------------------------------------
// [MỚI] TĂNG ĐỘ NHẠY CẢM BIẾN LỬA (FLAME SENSITIVITY PRESETS)
// Chọn mức độ nhạy:
//   1: CỰC NHẠY (Bắt bật lửa cách 0.8m - 1.5m, bắt tia lửa chớp) -> [KHUYẾN NGHỊ]
//   2: TIÊU CHUẨN (Bắt lửa khoảng 0.4m - 0.7m trong phòng)
//   3: CHỐNG NHIỄU (Chỉ báo khi lửa to hoặc rất gần < 30cm, tránh đèn halogen)
#define FLAME_SENSITIVITY_MODE 1

#if FLAME_SENSITIVITY_MODE == 1
  const int NL_FIRE_MAX   = 2800; // ADC < 2800: BÁO CHÁY NGAY (còi 10Hz) - CỰC KỲ NHẠY!
  const int NL_WARN_MAX   = 3600; // 2800 <= ADC <= 3600: Cảnh báo sớm (còi 1Hz)
#elif FLAME_SENSITIVITY_MODE == 2
  const int NL_FIRE_MAX   = 2200; // ADC < 2200: Báo Cháy
  const int NL_WARN_MAX   = 3300; // 2200 <= ADC <= 3300: Cảnh báo sớm
#else
  const int NL_FIRE_MAX   = 1600; // ADC < 1600: Báo Cháy
  const int NL_WARN_MAX   = 3000; // 1600 <= ADC <= 3000: Cảnh báo sớm
#endif

// ======================================================================================
// 4. KHỞI TẠO ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC
// ======================================================================================
DHT dht(PIN_DHT, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Thử địa chỉ I2C 0x27 (hoặc 0x3F)

float val_RD = 0.0;     // Độ lớn rung động (g)
float val_ND = 25.0;    // Nhiệt độ (°C)
float val_DA = 60.0;    // Độ ẩm (%)
int   val_NL = 4095;    // Giá trị ngọn lửa (ADC 0-4095)

int level_RD = 0;       // 0: An toàn, 1: Cảnh báo nhẹ (1Hz), 2: Nguy hiểm (10Hz)
int level_ND = 0;
int level_NL = 0;
int system_max_level = 0;

bool is_mpu6050 = false;
bool is_adxl345 = false;

unsigned long timer_sample    = 0;
unsigned long timer_blink     = 0;
unsigned long timer_heartbeat = 0;
unsigned long last_wifi_retry = 0;
bool toggle_state = false;

String last_alert_event = "safe";
uint32_t packet_seq = 0;

// ======================================================================================
// 4.1 HÀM ĐIỀU KHIỂN & CHẨN ĐOÁN CÒI (BUZZER DRIVER)
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

// Bíp còi kiểm tra chẩn đoán
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

// Đọc cảm biến lửa lấy mẫu trung bình 8 lần để lọc nhiễu và bắt tia lửa siêu nhạy
int readFlameFiltered() {
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_FLAME_ANALOG);
    delayMicroseconds(50);
  }
  return (int)(sum / 8);
}

// ======================================================================================
// 5. TỰ ĐỘNG NHẬN DIỆN MPU6050 HOẶC ADXL345 QUA I2C
// ======================================================================================
bool initAccelerometer() {
  // 1. Kiểm tra MPU-6050 (Địa chỉ 0x68) - Module GY-521 trên tay bạn
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() == 0) {
    Wire.beginTransmission(0x68);
    Wire.write(0x6B); // PWR_MGMT_1
    Wire.write(0x00); // Đánh thức MPU-6050
    Wire.endTransmission();
    is_mpu6050 = true;
    Serial.println(F("[SENSOR] Da nhan dien MPU-6050 (GY-521)!"));
    return true;
  }

  // 2. Kiểm tra ADXL345 (Địa chỉ 0x53)
  Wire.beginTransmission(0x53);
  if (Wire.endTransmission() == 0) {
    Wire.beginTransmission(0x53);
    Wire.write(0x2D); // POWER_CTL
    Wire.write(0x08); // Bật chế độ đo
    Wire.endTransmission();
    is_adxl345 = true;
    Serial.println(F("[SENSOR] Da nhan dien ADXL-345!"));
    return true;
  }

  return false;
}

float readVibrationG() {
  if (is_mpu6050) {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    if (Wire.requestFrom((uint8_t)0x68, (size_t)6) == 6) {
      int16_t ax = (Wire.read() << 8) | Wire.read();
      int16_t ay = (Wire.read() << 8) | Wire.read();
      int16_t az = (Wire.read() << 8) | Wire.read();
      float gx = ax / 16384.0f;
      float gy = ay / 16384.0f;
      float gz = az / 16384.0f;
      float total = sqrt(gx * gx + gy * gy + gz * gz);
      return fabs(total - 1.0f); // Độ rung lệch so với trọng lực 1g
    }
  } else if (is_adxl345) {
    Wire.beginTransmission(0x53);
    Wire.write(0x32);
    Wire.endTransmission(false);
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
// 6. KIỂM TRA KẾT NỐI HỆ THỐNG
// ======================================================================================
bool checkSystemConnection() {
  if (!is_mpu6050 && !is_adxl345) return false;
  float t = dht.readTemperature();
  if (isnan(t)) return false;
  return true;
}

// ======================================================================================
// 7. KẾT NỐI WI-FI (VỚI DNS GOOGLE ĐỂ CHỐNG CHẶN TELEGRAM TẠI VIỆT NAM)
// ======================================================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print(F("\n[WIFI] Dang ket noi mang: "));
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  delay(150);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Giữ radio Wi-Fi luôn bật công suất tối đa

  // Cấu hình DNS Google (8.8.8.8) và Cloudflare (1.1.1.1) chống lỗi nhà mạng chặn Telegram
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  // Chờ tối đa 15 giây
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(400);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n[WIFI] >>> KET NOI THANH CONG! IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\n[WIFI] Chua ket noi duoc. Hay kiem tra dung ten va pass Wi-Fi 2.4GHz!"));
  }
}

// ======================================================================================
// 7.1 HÀM GỬI TÍN HIỆU TRỰC TIẾP TỚI TELEGRAM BOT (ĐỘC LẬP 100%)
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

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[TELEGRAM] Khong the gui tin vi ESP32 chua ket noi duoc Wi-Fi!"));
    return;
  }

  Serial.println(F("\n[TELEGRAM TRUC TIEP] >>> Dang gui ban tin toi Bot Telegram..."));

  WiFiClientSecure client;
  client.setInsecure(); // Bỏ qua SSL fingerprint để không lỗi chứng chỉ CA
  client.setTimeout(7000);

  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) + "\",";
    jsonPayload += "\"text\":\"" + escapeJson(message) + "\",";
    jsonPayload += "\"parse_mode\":\"HTML\"";
    jsonPayload += "}";

    int httpCode = https.POST(jsonPayload);

    if (httpCode == 200 || httpCode == 201) {
      Serial.printf("[TELEGRAM TRUC TIEP] >>> GUI THANH CONG (HTTP %d)! Ban tin da vao dien thoai.\n", httpCode);
    } else {
      Serial.printf("[TELEGRAM TRUC TIEP] >>> Gui that bai (HTTP: %d). He thong tiep tuc gui qua VPS.\n", httpCode);
    }
    https.end();
  } else {
    Serial.println(F("[TELEGRAM TRUC TIEP] >>> Khong the khoi tao ket noi HTTPS toi api.telegram.org!"));
  }
}

// ======================================================================================
// 7.2 HÀM GỬI TELEMETRY TỚI VPS DỰ PHÒNG
// ======================================================================================
void sendTelemetryToVPS(const String& eventName, const String& lcdMsg) {
#if !ENABLE_TELEGRAM
  return;
#endif

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(VPS_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);
  http.setTimeout(4000);

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
  payload += "\"sensor_fault\":false,";
  payload += "\"alarm_state\":\"" + String(system_max_level == 2 ? "ALARM" : (system_max_level == 1 ? "SUSPECT" : "NORMAL")) + "\",";
  payload += "\"alarm_latched\":false,";
  payload += "\"event\":\"" + eventName + "\",";
  payload += "\"lcd_message\":\"" + lcdMsg + "\"";
  payload += "}";

  int code = http.POST(payload);
  if (code == 200) {
    Serial.printf("[VPS] Dong bo su kien '%s' thanh cong (HTTP 200).\n", eventName.c_str());
  } else {
    Serial.printf("[VPS] Dong bo su kien '%s' ket qua HTTP %d.\n", eventName.c_str(), code);
  }
  http.end();
}

// ======================================================================================
// 7.3 HÀM PHÁT CẢNH BÁO TỔNG HỢP (GỬI SONG SONG CẢ TELEGRAM TRỰC TIẾP LẪN VPS)
// ======================================================================================
void broadcastAlert(const String& eventName, const String& lcdMsg, const String& teleMsg) {
  if (eventName == last_alert_event) return;
  last_alert_event = eventName;

  Serial.printf("\n[ALERT] Phat canh bao su kien: %s (LCD: %s)\n", eventName.c_str(), lcdMsg.c_str());

  // 1. Gửi thẳng trực tiếp vào Telegram Bot
  sendDirectToTelegram(teleMsg);

  // 2. Gửi đồng thời tới VPS dự phòng
  sendTelemetryToVPS(eventName, lcdMsg);
}

// ======================================================================================
// 8. SETUP
// ======================================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_BUTTON_TEST, INPUT_PULLUP);
  pinMode(PIN_FLAME_ANALOG, INPUT);

  setBuzzer(false);
  digitalWrite(PIN_LED_ALERT, LOW);

  // [KIỂM TRA CÒI KHI BẬT NGUỒN - SELF TEST]
  Serial.println(F("\n[TEST COI] Dang phat 2 tieng bip khoi dong (Self-Test)..."));
  beepBuzzer(2, 100, 80);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CANH BAO THIEN TAI");
  lcd.setCursor(0, 1);
  lcd.print("KHOI TAO HE THONG");

  // Khởi tạo cảm biến
  initAccelerometer();
  dht.begin();

  delay(1200);

  // Kết nối Wi-Fi
  connectWiFi();

#if ENABLE_TELEGRAM
  if (WiFi.status() == WL_CONNECTED) {
    String startupMsg = "✅ <b>[HỆ THỐNG CẢNH BÁO KHỞI ĐỘNG]</b>\n"
                        "📍 <b>Thiết bị:</b> " + String(DEVICE_ID) + "\n"
                        "📶 <b>IP Wi-Fi:</b> " + WiFi.localIP().toString() + "\n"
                        "🔔 <b>Còi & Cảm biến:</b> Sẵn sàng giám sát 24/7!";
    sendDirectToTelegram(startupMsg);
    sendTelemetryToVPS("recovery", "He Thong San Sang");
  }
#endif

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG SAN SANG");
  lcd.setCursor(0, 1);
  lcd.print("TRANG THAI: OK   ");
  delay(1000);
  lcd.clear();
}

// ======================================================================================
// 9. LOOP
// ======================================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. NHẬN LỆNH TEST CÒI TỪ SERIAL MONITOR (Gõ 't' hoặc 'b' để test còi & test Telegram)
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 't' || cmd == 'T' || cmd == 'b' || cmd == 'B') {
      Serial.println(F("\n[TEST SERIAL] >>> Kich hoat coi keu va gui tin test len Telegram..."));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TEST HE THONG...");
      lcd.setCursor(0, 1);
      lcd.print("COI: DANG KEU...");
      setBuzzer(true);
      digitalWrite(PIN_LED_ALERT, HIGH);

#if ENABLE_TELEGRAM
      String testMsg = "🧪 <b>[TEST HỆ THỐNG QUA SERIAL]</b>\n"
                       "📍 <b>Thiết bị:</b> " + String(DEVICE_ID) + "\n"
                       "🔔 Lệnh test còi & đường truyền Telegram hoạt động tốt!";
      sendDirectToTelegram(testMsg);
#endif

      delay(1000);
      setBuzzer(false);
      digitalWrite(PIN_LED_ALERT, LOW);
      Serial.println(F("[TEST SERIAL] >>> Da tat coi test."));
      lcd.clear();
    }
  }

  // 2. NÚT NHẤN TEST HỆ THỐNG TẠI CHỖ (GPIO 15)
  if (digitalRead(PIN_BUTTON_TEST) == LOW) {
    Serial.println(F("[BUTTON] Nut Test duoc bam -> Phat 3 tieng bip va gui tin test len Telegram!"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TEST COI & LED  ");
    lcd.setCursor(0, 1);
    lcd.print("DANG KEU BEEP!  ");
    beepBuzzer(3, 150, 100);

#if ENABLE_TELEGRAM
    String btnMsg = "🧪 <b>[KIỂM TRA HỆ THỐNG - NÚT TEST]</b>\n"
                    "📍 <b>Thiết bị:</b> " + String(DEVICE_ID) + "\n"
                    "🔔 Đã nhấn nút kiểm tra còi & cảm biến tại chỗ.\n"
                    "✅ Tín hiệu gửi đến Telegram hoàn toàn bình thường!";
    sendDirectToTelegram(btnMsg);
    sendTelemetryToVPS("recovery", "Nhan Nut Test");
#endif
    delay(200);
  }

  // 3. CHU KỲ QUÉT CẢM BIẾN VÀ THỰC THI LƯU ĐỒ (MỖI 100ms)
  if (currentMillis - timer_sample >= 100) {
    timer_sample = currentMillis;

    // Kiểm tra kết nối cảm biến
    bool isConnected = checkSystemConnection();
    if (!isConnected) {
      lcd.setCursor(0, 0);
      lcd.print("THONG BAO LOI   ");
      lcd.setCursor(0, 1);
      lcd.print("MAT KET NOI CB! ");
      setBuzzer(false);
      digitalWrite(PIN_LED_ALERT, HIGH);
      broadcastAlert("sensor_fault", "THONG BAO LOI", "⚠️ <b>[CẢNH BÁO LỖI CẢM BIẾN]</b> Mất kết nối I2C hoặc DHT!");
      return;
    }

    // Đọc cảm biến
    val_RD = readVibrationG();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) val_ND = t;
    if (!isnan(h)) val_DA = h;
    val_NL = readFlameFiltered(); // Đọc cảm biến lửa qua bộ lọc lấy mẫu siêu nhạy

    // [NHÁNH 1: RUNG ĐỘNG RĐ]
    if (val_RD < RD_SAFE_MAX) {
      level_RD = 0; // An toàn
    } else if (val_RD <= RD_WARN_MAX) {
      level_RD = 1; // 0.2g - 0.5g: Rung nhẹ
    } else {
      level_RD = 2; // > 0.5g: Động đất
    }

    // [NHÁNH 2: NHIỆT ĐỘ NĐ]
    if (val_ND > ND_WARN_MAX) {
      level_ND = 2; // > 45°C: Nhiệt độ cao
    } else if (val_ND >= ND_SAFE_MAX) {
      level_ND = 1; // 38°C - 45°C: Nhiệt độ TB
    } else {
      level_ND = 0; // < 38°C: An toàn
    }

    // [NHÁNH 3: NGỌN LỬA NL]
    if (val_NL < NL_FIRE_MAX) {
      level_NL = 2; // < NL_FIRE_MAX: Cảnh báo cháy
    } else if (val_NL <= NL_WARN_MAX) {
      level_NL = 1; // NL_FIRE_MAX - NL_WARN_MAX: Cảnh báo sớm
    } else {
      level_NL = 0; // > NL_WARN_MAX: An toàn
    }

    // Tổng hợp mức độ ưu tiên cao nhất
    system_max_level = max(level_RD, max(level_ND, level_NL));

    // Xác định nội dung hiển thị LCD và bản tin Telegram
    String lcdLine2Msg = "An Toan         ";
    String currentEvent = "safe";
    String teleMsg = "";

    if (system_max_level == 2) {
      if (level_RD == 2) {
        lcdLine2Msg = "! DONG DAT !    ";
        currentEvent = "earthquake";
        teleMsg = "🚨🚨 <b>[CẢNH BÁO ĐỘNG ĐẤT NGUY HIỂM]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "⚠️ <b>Rung động:</b> " + String(val_RD, 2) + "g (> 0.5g)!\n"
                  "📢 <b>Còi & LED:</b> Kêu 10Hz khẩn cấp!\n"
                  "⚠️ <i>Hãy tìm nơi trú ẩn an toàn ngay lập tức!</i>";
      } else if (level_NL == 2) {
        lcdLine2Msg = "! CANH BAO CHAY!";
        currentEvent = "fire_alarm";
        teleMsg = "🔥🚨 <b>[CẢNH BÁO CHÁY KHẨN CẤP]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "🔥 <b>Ngọn lửa:</b> Phát hiện! (ADC: " + String(val_NL) + " < " + String(NL_FIRE_MAX) + ")\n"
                  "🌡 <b>Nhiệt độ:</b> " + String(val_ND, 1) + " °C\n"
                  "📢 <b>Còi & LED:</b> Kêu 10Hz dồn dập!\n"
                  "⚠️ <i>Hãy sơ tán và xử lý chữa cháy khẩn cấp!</i>";
      } else {
        lcdLine2Msg = "! NHIET DO CAO !";
        currentEvent = "temp_high";
        teleMsg = "🌡🚨 <b>[CẢNH BÁO NHIỆT ĐỘ QUÁ CAO]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "🌡 <b>Nhiệt độ:</b> " + String(val_ND, 1) + " °C (> 45°C)!\n"
                  "📢 <b>Còi & LED:</b> Kêu 10Hz khẩn cấp!";
      }
    } else if (system_max_level == 1) {
      if (level_RD == 1) {
        lcdLine2Msg = "Rung nhe        ";
        currentEvent = "vibration_light";
        teleMsg = "⚠️ <b>[CẢNH BÁO RUNG NHẸ]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "ℹ️ Rung động: " + String(val_RD, 2) + "g (0.2g - 0.5g)\n"
                  "📢 Còi nhấp nháy 1Hz.";
      } else if (level_NL == 1) {
        lcdLine2Msg = "Canh bao som    ";
        currentEvent = "flame_early";
        teleMsg = "⚠️ <b>[CẢNH BÁO SỚM NGỌN LỬA]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "ℹ️ Phát hiện bức xạ lửa khả nghi (ADC: " + String(val_NL) + ")\n"
                  "📢 Còi nhấp nháy 1Hz cảnh báo sớm.";
      } else {
        lcdLine2Msg = "Nhiet do TB     ";
        currentEvent = "temp_medium";
        teleMsg = "⚠️ <b>[CẢNH BÁO NHIỆT ĐỘ TRUNG BÌNH]</b>\n"
                  "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                  "ℹ️ Nhiệt độ: " + String(val_ND, 1) + " °C (38°C - 45°C).";
      }
    } else {
      lcdLine2Msg = "An Toan         ";
      currentEvent = "safe";
      teleMsg = "✅ <b>[BÁO ĐỘNG ĐÃ ĐƯỢC XÓA - HỆ THỐNG AN TOÀN]</b>\n"
                "📍 <b>Khu vực:</b> " + String(DEVICE_ID) + "\n"
                "🔇 Còi & LED: ĐÃ TẮT.\n"
                "ℹ️ Các chỉ số đã trở lại bình thường an toàn.";
    }

    // Hiển thị LCD
    lcd.setCursor(0, 0);
    lcd.print("R:");
    lcd.print(val_RD, 1);
    lcd.print("g L:");
    lcd.print(val_NL);
    lcd.print(" T:");
    lcd.print((int)val_ND);
    lcd.print("C  ");

    lcd.setCursor(0, 1);
    lcd.print(lcdLine2Msg);

    // Gửi cảnh báo Telegram & VPS khi có thay đổi trạng thái
    broadcastAlert(currentEvent, lcdLine2Msg, teleMsg);

    // Xuất Serial Monitor
    Serial.printf("[DATA] RD: %.2fg | NL: %d | ND: %.1fC | Muc: %d | Event: %s\n",
                  val_RD, val_NL, val_ND, system_max_level, currentEvent.c_str());
  }

  // 4. TỰ ĐỘNG KẾT NỐI LẠI WI-FI VÀ GỬI NHỊP TIM (Mỗi 20 giây nếu mất mạng)
  if (currentMillis - timer_heartbeat >= 20000) {
    timer_heartbeat = currentMillis;
    if (WiFi.status() == WL_CONNECTED) {
      sendTelemetryToVPS("heartbeat", "An Toan");
    } else {
      connectWiFi();
    }
  }

  // 5. ĐIỀU KHIỂN CÒI & LED THEO TẦN SỐ LƯU ĐỒ
  if (system_max_level == 0) {
    // An toàn: Còi OFF, LED OFF
    setBuzzer(false);
    digitalWrite(PIN_LED_ALERT, LOW);
  } else if (system_max_level == 1) {
    // Mức 1 (1Hz): Chu kỳ 1000ms (500ms BẬT, 500ms TẮT)
    if (currentMillis - timer_blink >= 500) {
      timer_blink = currentMillis;
      toggle_state = !toggle_state;
      setBuzzer(toggle_state);
      digitalWrite(PIN_LED_ALERT, toggle_state ? HIGH : LOW);
    }
  } else if (system_max_level == 2) {
    // Mức 2 (10Hz): Chu kỳ 100ms (50ms BẬT, 50ms TẮT dồn dập)
    if (currentMillis - timer_blink >= 50) {
      timer_blink = currentMillis;
      toggle_state = !toggle_state;
      setBuzzer(toggle_state);
      digitalWrite(PIN_LED_ALERT, toggle_state ? HIGH : LOW);
    }
  }
}
