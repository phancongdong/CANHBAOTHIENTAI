# Hệ Thống Cảnh Báo Cháy ESP32 → VPS → Telegram

> **LƯU Ý AN TOÀN:**
> Dự án này là hệ thống nguyên mẫu IoT cảnh báo hỗ trợ, **không thay thế** các hệ thống báo cháy đạt chuẩn PCCC hoặc quy định pháp luật hiện hành.
> Còi báo động tại chỗ được thiết kế hoạt động độc lập 100% với Internet và mạng Wi-Fi.

## 1. Cấu Trúc Dự Án

```
CANHBAOTHIENTAI/
├── docs/
│   ├── spec.md             # Đặc tả kỹ thuật, schema v1, state machine, outbox
│   ├── hardware.md         # Kiểm kê phần cứng (Fail-Closed Hardware Gate)
│   ├── runbook.md          # Hướng dẫn vận hành, quản lý key, backup/restore
│   └── superpowers/plans/  # Bản kế hoạch chi tiết từng bước
├── firmware/               # Mã nguồn C++ PlatformIO cho vi điều khiển ESP32
│   ├── include/            # Header files (alarm_logic, event, config)
│   ├── src/                # Implementation (alarm_logic, queue, transport, buzzer)
│   └── test/               # Native unit tests (kiểm tra logic không cần phần cứng)
├── backend/                # Dịch vụ FastAPI & Ingest Transactional API
│   ├── app/                # Mã nguồn API, DB SQLite, Ingest, Worker Telegram
│   └── tests/              # Bộ test tự động pytest (contract, idempotency, retry)
├── deploy/                 # Cấu hình Docker, Caddy HTTPS reverse proxy
│   ├── Dockerfile          # Image non-root an toàn
│   ├── compose.yaml        # Điều phối API, Worker, Proxy Caddy
│   ├── Caddyfile           # Cấu hình HTTPS tự động
│   └── .env.example        # Mẫu biến môi trường (không chứa secret thật)
├── scripts/                # Tiện ích bổ trợ (giả lập thiết bị E2E, sao lưu DB)
└── README.md
```

## 2. Các Thành Phần Chính

1. **ESP32 Edge Device:**
   - Đọc cảm biến lửa hồng ngoại và cảm biến nhiệt độ.
   - Chạy State Machine cục bộ: Lọc nhiễu ánh sáng, chốt báo động, bật còi trực tiếp.
   - Hàng đợi bền vững (Durable Event Queue) lưu trữ sự kiện vào bộ nhớ Flash khi mất mạng.
   - Gửi sự kiện lên VPS qua giao thức HTTPS có xác thực chứng chỉ CA đầy đủ.

2. **VPS Backend & Ingest API:**
   - Xây dựng trên nền FastAPI và SQLite với cơ chế WAL.
   - Xử lý bản tin đảm bảo tính Idempotent qua bộ khóa `(device_id, boot_id, seq)`.
   - Ghi dữ liệu sự kiện, trạng thái thiết bị và hàng đợi gửi tin (Outbox) trong cùng 1 Transaction duy nhất.

3. **Telegram Notification Worker:**
   - Tiến trình worker riêng biệt, claim tin từ outbox với cơ chế lock/lease.
   - Gửi cảnh báo tức thời tới Telegram Bot với cơ chế exponential backoff và xử lý rate limit (HTTP 429).
   - Tự động cảnh báo khi thiết bị mất kết nối (Offline > 60s) và thông báo phục hồi (Recovery).

## 3. Khởi Chạy Nhanh Cho Nhà Phát Triển

### Chạy Backend Tests
```bash
cd backend
python -m pytest -v
```

### Chạy Giả Lập Thiết Bị E2E
```bash
python scripts/simulate_device.py --base-url http://127.0.0.1:8000 --device-id kitchen-01 --scenario alarm-cycle
```

Xem chi tiết hướng dẫn vận hành tại [docs/runbook.md](file:///C:/CANHBAOTHIENTAI/docs/runbook.md).
