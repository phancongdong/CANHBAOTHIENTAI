# Đặc Tả Kỹ Thuật Hệ Thống Cảnh Báo Cháy (ESP32 → VPS → Telegram)

Phiên bản: 1.0.0 (Schema Version 1)

---

## 1. Mục Tiêu & Phạm Vi An Toàn

1. **Nguyên mẫu cảnh báo phụ trợ:** Hệ thống này là nguyên mẫu IoT hỗ trợ thông tin, **tuyệt đối không thay thế** các đầu báo cháy tiêu chuẩn theo quy chuẩn PCCC (TCVN / UL / EN54) hoặc hệ thống chữa cháy chuyên dụng.
2. **Ưu tiên vận hành cục bộ (Local Autonomy):** Còi báo cháy tại chỗ phải hoạt động độc lập 100% với trạng thái kết nối mạng (Wi-Fi, Internet, VPS, Telegram). Khi có lửa hoặc nhiệt độ vượt ngưỡng báo động, còi phải kêu ngay lập tức tại chỗ.
3. **Fail-Closed & Bảo mật:**
   - Không tắt kiểm tra chứng chỉ TLS (`setInsecure()` bị cấm triệt để).
   - Bot Token và Chat ID chỉ lưu tại VPS, không nạp vào firmware ESP32.
   - ESP32 xác thực với VPS bằng Token Bearer riêng của từng thiết bị.

---

## 2. Hợp Đồng Dữ Liệu (Event & Ingest API Contract)

### 2.1 JSON Schema v1

Mỗi bản tin gửi từ ESP32 lên VPS qua endpoint `POST /api/v1/telemetry` tuân theo cấu trúc JSON:

```json
{
  "schema_version": 1,
  "device_id": "kitchen-01",
  "boot_id": "boot-000017",
  "seq": 42,
  "uptime_ms": 123456,
  "sample_age_ms": 0,
  "temperature_c": 48.2,
  "flame_detected": true,
  "sensor_fault": false,
  "alarm_state": "ALARM",
  "alarm_latched": true,
  "event": "alarm_started",
  "incident_id": "inc-000042"
}
```

### 2.2 Chi Tiết Các Trường

| Tên trường | Kiểu dữ liệu | Bắt buộc | Mô tả |
| :--- | :--- | :--- | :--- |
| `schema_version` | integer | Có | Phiên bản schema, giá trị cố định là `1`. |
| `device_id` | string (<= 64) | Có | Định danh duy nhất của thiết bị (vd: `kitchen-01`). |
| `boot_id` | string (<= 32) | Có | Định danh phiên khởi động (tăng dần lưu trong NVS, vd: `boot-000017`). |
| `seq` | integer (>= 0) | Có | Số thứ tự tăng dần liên tục trong cùng 1 phiên boot. |
| `uptime_ms` | integer (>= 0) | Có | Số mili-giây ESP32 đã chạy kể từ lúc khởi động. |
| `sample_age_ms` | integer (>= 0) | Có | Thời gian kể từ lúc lấy mẫu cảm biến đến lúc đóng gói bản tin (ms). |
| `temperature_c` | float / null | Không | Nhiệt độ đo được (°C). Giá trị là `null` nếu cảm biến nhiệt độ bị lỗi. |
| `flame_detected` | bool / null | Không | `true` nếu phát hiện lửa hồng ngoại. `null` nếu cảm biến lửa bị lỗi. |
| `sensor_fault` | boolean | Có | `true` nếu ít nhất 1 cảm biến bị đứt dây/mất giao tiếp/giá trị bất thường. |
| `alarm_state` | string | Có | Một trong: `NORMAL`, `SUSPECT`, `ALARM`, `SENSOR_FAULT`. |
| `alarm_latched` | boolean | Có | `true` nếu hệ thống đang giữ trạng thái chốt báo động (cần reset tay hoặc điều kiện clear). |
| `event` | string | Có | Một trong: `heartbeat`, `alarm_started`, `alarm_cleared`, `sensor_fault`, `sensor_recovered`. |
| `incident_id` | string / null | Không | Mã định danh sự cố (vd: `inc-000042`), giữ nguyên suốt chu kỳ ALARM cho đến khi clear. |

### 2.3 Idempotency & Thứ Tự Bản Tin

- **Khóa duy nhất (Idempotency Key):** Cặp `(device_id, boot_id, seq)`.
- Nếu VPS nhận lại cùng bộ `(device_id, boot_id, seq)`:
  - Nếu payload hash khớp: Trả lời HTTP `200 OK` với body `{"accepted": true, "duplicate": true}` và KHÔNG tạo bản tin Telegram mới.
  - Nếu payload hash khác: Trả lời HTTP `409 Conflict`.
- **Thứ tự thời gian:** VPS đánh giá thời gian nhận bản tin (`server_receive_time`), không tin cậy đồng hồ RTC của ESP32 để xác định thời gian tuyệt đối.

---

## 3. State Machine Báo Động Cục Bộ

### 3.1 Các Trạng Thái (AlarmState)

1. `NORMAL`: Trạng thái bình thường. Không có lửa, nhiệt độ trong khoảng an toàn, cảm biến hoạt động tốt.
2. `SUSPECT`: Phát hiện lửa chớp nhoáng hoặc nhiệt độ chạm ngưỡng cảnh báo, đang trong thời gian xác minh (`confirm_ms`). Còi chưa kích hoạt để chống báo động giả.
3. `ALARM`: Đã xác nhận có cháy (lửa liên tục qua `confirm_ms` HOẶC nhiệt độ vượt `temp_alarm_c`).
   - Còi kêu liên tục/ngắt quãng cường độ cao ngay lập tức.
   - Trạng thái chốt (`alarm_latched = true`).
   - Kể cả khi tắt lửa ngay sau đó, còi và trạng thái ALARM vẫn duy trì cho đến khi đáp ứng điều kiện clear an toàn.
4. `SENSOR_FAULT`: Cảm biến mất kết nối (đứt cáp, đọc CRC lỗi, giá trị out of range). Còi kêu bíp cảnh báo lỗi cảm biến (chu kỳ ngắn), phát sự kiện `sensor_fault`.

### 3.2 Quy Tắc Chuyển Trạng Thái (Transitions)

```
[NORMAL]  ----(phát hiện lửa/nhiệt độ tăng)----> [SUSPECT]
   |                                                |
   |                                    (đủ confirm_ms)
   |                                                v
   |<---(tín hiệu tắt trước confirm_ms)-------   [ALARM] (LATCHED)
   |                                                |
   |                                    (nhiệt < temp_clear_c & không lửa
   |                                     ổn định đủ clear_ms + reset tay)
   |<-----------------------------------------------+
   |
   +-----(lỗi phần cứng cảm biến)-----> [SENSOR_FAULT]
   |<----(cảm biến phục hồi ổn định)----+
```

### 3.3 Ngưỡng Đo & Tham Số Thời Gian (Cần Hiệu Chuẩn Thực Tế)

*Lưu ý: Các giá trị dưới đây là giá trị cấu hình tham chiếu, bắt buộc phải hiệu chuẩn sau khi xác minh linh kiện thực tế.*

- `temp_warn_c`: 45.0 °C (ngưỡng nghi ngờ/cảnh báo sớm).
- `temp_alarm_c`: 55.0 °C (ngưỡng kích hoạt báo cháy khẩn cấp).
- `temp_clear_c`: 38.0 °C (ngưỡng nhiệt độ an toàn để cho phép xóa báo động, hysteresis = 17°C).
- `confirm_ms`: 1000 ms (thời gian duy trì tín hiệu lửa/nhiệt để xác nhận báo động thật, lọc nhiễu ánh sáng).
- `clear_ms`: 10000 ms (thời gian tín hiệu phải bình thường liên tục trước khi cho phép clear).
- `heartbeat_interval_ms`: 30000 ms (gửi nhịp tim định kỳ 30 giây khi bình thường).

---

## 4. Hàng Đợi Bền Vững (Durable Offline Queue) Trên ESP32

- Khi mất Wi-Fi hoặc VPS không phản hồi, ESP32 lưu trữ sự kiện vào bộ đệm NVS/Flash.
- **Ưu tiên sự kiện:** Sự kiện `alarm_started`, `alarm_cleared`, `sensor_fault` có độ ưu tiên cao nhất, tuyệt đối không bị drop.
- **Chính sách hàng đợi:**
  - Dung lượng hàng đợi tối đa: 50 sự kiện.
  - Khi hàng đợi đầy: Gộp/bỏ qua sự kiện `heartbeat` cũ nhất, tăng bộ đếm `dropped_heartbeats`.
  - Khi kết nối mạng phục hồi: Gửi lại tuần tự theo FIFO có exponential backoff và jitter (tránh bão request).

---

## 5. Xử Lý Outbox & Thông Báo Telegram Trên VPS

1. **Transactional Ingest:** Khi nhận telemetry tại `POST /api/v1/telemetry`:
   - Ghi bản ghi vào bảng `events`.
   - Cập nhật bảng `device_state`.
   - Tạo bản ghi trong bảng `outbox` (nếu là `alarm_started`, `alarm_cleared`, `sensor_fault`, `sensor_recovered`).
   - Tất cả 3 thao tác nằm trong cùng 1 SQLite Transaction. Nếu lỗi, rollback toàn bộ và trả về 500/503.
2. **Worker Độc Lập:**
   - Worker chạy tiến trình riêng biệt, định kỳ claim các row `pending` trong `outbox` bằng cơ chế lease (khóa chống trùng lặp giữa các worker).
   - Gửi tin nhắn qua Telegram Bot API (sử dụng HTTP client có timeout và retry).
   - Xử lý mã HTTP `429 Too Many Requests`: Đọc header `Retry-After` và tạm hoãn đúng thời gian yêu cầu.
   - Đánh dấu `sent` chỉ khi Telegram API phản hồi thành công (`ok: true`).
3. **Chống Spam & Giới Hạn Tần Suất:**
   - Heartbeat định kỳ KHÔNG gửi tin nhắn Telegram.
   - Khi đang trong trạng thái `ALARM`, cho phép gửi tin nhắc nhở (Reminder) tối đa 1 lần mỗi 5 phút nếu sự cố chưa được dập tắt/clear.
   - Sự kiện mất kết nối (Offline): Nếu VPS không nhận được heartbeat sau 60 giây, gửi thông báo cảnh báo thiết bị Offline một lần duy nhất. Khi có tin mới, gửi thông báo phục hồi (Online) một lần duy nhất.
