# Sổ Tay Vận Hành & Khắc Phục Sự Cố (Runbook)

Hệ thống cảnh báo cháy: ESP32 -> VPS -> Telegram

---

## 1. Kiến Trúc & Luồng Dữ Liệu

- **Edge:** ESP32 đọc cảm biến chu kỳ 100ms, điều khiển còi trực tiếp tại chỗ bằng state machine không phụ thuộc mạng. Khi có sự kiện hoặc heartbeat (30s), gửi HTTP POST lên VPS kèm Bearer token.
- **Proxy & TLS:** Caddy reverse proxy lắng nghe cổng 80/443, tự động cấp phát và gia hạn chứng chỉ Let's Encrypt / ZeroSSL, chuyển tiếp `/api/*` tới backend FastAPI.
- **Backend API:** FastAPI chạy non-root trong container, ghi `events`, `device_state`, và `outbox` trong cùng 1 SQLite transaction duy nhất (ACID).
- **Notification Worker:** Tiến trình chạy nền claim bản ghi `pending` từ `outbox`, gửi thông báo tới Telegram Bot API có backoff retry và rate-limit handling.

---

## 2. Quản Lý Thông Tin Xác Thực (Credentials Management)

### 2.1 Cấu Hình Biến Môi Trường VPS (`deploy/.env`)

File `.env` trên VPS lưu trữ các thông tin nhạy cảm:

```env
PUBLIC_DOMAIN=canhbao.yourdomain.com
TELEGRAM_BOT_TOKEN=8622569426:AAGOlFk9Zfhxlj6d-WGSGb9C-GCdcYLM-9U
TELEGRAM_CHAT_ID=6130858384
DEVICE_CREDENTIALS_FILE=/data/device_credentials.json
DATABASE_PATH=/data/fire_alarm.db
```

### 2.2 Quản Lý Token Thiết Bị (`device_credentials.json`)

Mỗi thiết bị ESP32 sở hữu một token xác thực riêng biệt:

```json
{
  "kitchen-01": "$argon2id$v=19$m=65536,t=3,p=4$..."
}
```

- **Thêm thiết bị mới:** Tạo chuỗi token ngẫu nhiên tối thiểu 32 ký tự (vd: `openssl rand -hex 32`), tính hash Argon2/SHA-256 và thêm vào file credentials trên server. Cấu hình token thô tương ứng vào file `config.h` của ESP32.
- **Thu hồi/Thay token khẩn cấp:** Xóa hoặc cập nhật hash tương ứng trong `device_credentials.json`, reload service FastAPI không cần downtime:
  ```bash
  docker compose exec api kill -HUP 1
  ```

---

## 3. Quy Trình Sao Lưu & Phục Hồi Dữ Liệu (Backup & Restore)

SQLite đang ở chế độ WAL (Write-Ahead Logging), **tuyệt đối không dùng lệnh `cp` trực tiếp file `.db`** khi hệ thống đang chạy vì có thể làm hỏng tính toàn vẹn dữ liệu.

### 3.1 Sao Lưu An Toàn (Online Hot Backup)

Sử dụng script `scripts/backup_db.py` tích hợp sẵn phương thức `sqlite3.Connection.backup()`:

```bash
python scripts/backup_db.py --database /data/fire_alarm.db --output /backups/fire_alarm_$(date +%Y%m%d_%H%M%S).db
```

Script sẽ tự động chạy lệnh kiểm tra toàn vẹn:
```sql
PRAGMA integrity_check;
```
Chỉ khi kết quả trả về `ok`, file backup mới được đổi tên chính thức.

### 3.2 Phục Hồi Dữ Liệu (Restore)

1. Tạm dừng dịch vụ:
   ```bash
   docker compose down
   ```
2. Lưu bản sao DB hiện tại:
   ```bash
   mv /data/fire_alarm.db /data/fire_alarm.db.corrupt_$(date +%s)
   ```
3. Khôi phục từ bản backup đã xác minh:
   ```bash
   cp /backups/fire_alarm_selected.db /data/fire_alarm.db
   ```
4. Kiểm tra tính toàn vẹn và khởi động lại:
   ```bash
   sqlite3 /data/fire_alarm.db "PRAGMA integrity_check;"
   docker compose up -d
   ```

---

## 4. Xử Lý Sự Cố & Vận Hành Khẩn Cấp (Troubleshooting)

### 4.1 Lỗi Telegram Rate Limit (HTTP 429)

- **Hiện tượng:** Worker log xuất hiện cảnh báo `Telegram API returned 429 Too Many Requests`.
- **Hành vi hệ thống:** Worker tự động trích xuất trường `parameters.retry_after` trong JSON phản hồi của Telegram và tạm hoãn (sleep) đúng số giây yêu cầu trước khi thử lại bản ghi đang xét. Hàng đợi outbox không bị mất dữ liệu.

### 4.2 Thiết Bị Báo "Offline" Trên Telegram

- **Nguyên nhân:** VPS không nhận được heartbeat hoặc telemetry nào từ thiết bị trong hơn 60 giây.
- **Các bước kiểm tra tại chỗ:**
  1. Kiểm tra nguồn cấp ESP32 (LED nguồn có sáng không).
  2. Kiểm tra bộ phát Wi-Fi tại khu vực đặt thiết bị.
  3. Kiểm tra log serial của ESP32 qua cổng USB để xem mã lỗi kết nối Wi-Fi hoặc mã lỗi TLS/HTTP.
  4. Xác nhận còi tại chỗ vẫn hoạt động: đưa nguồn nhiệt/bật lửa thử nghiệm trước cảm biến (chú ý an toàn PCCC).

### 4.3 Khắc Phục Báo Động Giả (False Alarm)

- **Nguyên nhân:** Cảm biến lửa hồng ngoại bị ánh nắng trực tiếp, đèn halogen hoặc phản chiếu gây nhiễu.
- **Khắc phục:**
  1. Nhấn nút xóa báo động tại chỗ (Reset Button) trên thiết bị.
  2. Điều chỉnh biến trở độ nhạy trên module cảm biến lửa (xoay ngược chiều kim đồng hồ để giảm độ nhạy).
  3. Lắp thêm chụp che hướng quang học (optical shield) để giới hạn góc nhìn của cảm biến lửa vào đúng khu vực cần giám sát.

---

## 5. Bảng Tiêu Chí Nghiệm Thu Hệ Thống (Acceptance Checklist)

Trước khi đưa hệ thống vào giám sát thực tế, người vận hành phải tích đủ các mục sau:

- [ ] **DNS & HTTPS:** Tên miền trỏ đúng IP VPS, Caddy cấp chứng chỉ TLS Let's Encrypt thành công, không dùng tự ký.
- [ ] **Tường lửa (Firewall):** VPS chỉ mở cổng 22 (SSH), 80 (HTTP), 443 (HTTPS). Cổng SQLite/FastAPI nội bộ tuyệt đối không public ra ngoài Internet.
- [ ] **Bảo mật Firmware:** Firmware ESP32 nạp Root CA chuẩn (ISRG Root X1), cấm gọi `setInsecure()`.
- [ ] **Vận hành độc lập:** Ngắt hoàn toàn Wi-Fi/Internet, thử kích hoạt lửa -> Còi tại chỗ kêu ngay lập tức không trễ.
- [ ] **Bộ đệm Offline:** Khi ngắt mạng, sự kiện báo động được lưu vào bộ nhớ Flash. Khi kết nối lại Wi-Fi, sự kiện được đồng bộ lên VPS đầy đủ và Telegram nhận được thông báo.
- [ ] **Chống trùng lặp (Idempotency):** Giả lập gửi lặp lại 10 lần cùng 1 bản tin -> VPS chỉ chấp nhận 1 lần và Telegram chỉ gửi 1 tin thông báo.
- [ ] **Chống spam cảnh báo:** Heartbeat không gửi tin Telegram. Cảnh báo ALARM lặp lại tối đa 1 lần mỗi 5 phút nếu đám cháy chưa clear.
- [ ] **Sao lưu định kỳ:** Đã cấu hình cron job chạy `backup_db.py` hàng ngày và lưu trữ sang vị trí độc lập với VPS.
