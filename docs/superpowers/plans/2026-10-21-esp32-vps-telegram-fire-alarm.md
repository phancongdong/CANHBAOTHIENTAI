# Hệ thống cảnh báo cháy ESP32 → VPS → Telegram Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Chỉ thực thi sau khi người dùng duyệt; không tự tạo commit hoặc triển khai VPS.

**Goal:** Xây dựng nguyên mẫu cảnh báo cháy trong nhà: ESP32 đọc cảm biến lửa và nhiệt độ, bật còi độc lập Internet, gửi sự kiện qua HTTPS lên VPS và gửi cảnh báo Telegram có retry bền vững.

**Architecture:** ESP32 chạy state machine cục bộ, lọc nhiễu, xác nhận báo động, điều khiển còi và lưu hàng đợi sự kiện khi mất mạng. VPS chạy FastAPI và SQLite; API ghi telemetry, trạng thái thiết bị và outbox trong cùng transaction rồi mới ACK. Worker riêng gửi Telegram; Docker Compose và Caddy cung cấp HTTPS, health check và volume dữ liệu.

**Tech Stack:** PlatformIO, Arduino framework cho ESP32, C++; Python 3.12, FastAPI, Pydantic, HTTPX, SQLite, pytest; Docker Compose, Caddy; Telegram Bot API.

**Spec:** `docs/spec.md` sẽ được tạo ở Task 1 từ hợp đồng giao tiếp và yêu cầu vận hành dưới đây; executor phải đọc spec trước các task sau.

## Global Constraints

- Đây là nguyên mẫu bổ sung; không thay thế đầu báo cháy đạt chuẩn, quy định PCCC hoặc hệ thống bảo vệ sinh mạng.
- Không điều khiển chữa cháy tự động. Còi tại chỗ phải hoạt động khi mất Wi-Fi, mất VPS hoặc Telegram.
- Người dùng bổ sung model phần cứng sau khi plan hoàn tất; trước khi đấu dây phải xác minh model ESP32, cảm biến lửa, cảm biến nhiệt độ, còi, điện áp logic, dòng tải và datasheet.
- Không gán GPIO, sơ đồ nguồn hoặc ngưỡng nhiệt độ trước khi xác minh phần cứng. Không đưa tín hiệu 5 V vào GPIO ESP32; không nối còi tải lớn trực tiếp vào GPIO.
- Cảm biến lửa có thể bị ánh sáng gây nhiễu và có vùng quan sát hữu hạn; lửa+nhiệt không phát hiện đáng tin cậy mọi đám cháy âm ỉ. Khuyến nghị dùng thêm đầu báo khói đạt chuẩn.
- TLS phải kiểm tra CA và hostname; cấm `setInsecure()` hoặc tắt xác minh chứng chỉ.
- Telegram bot token chỉ tồn tại trên VPS. Không đưa Wi-Fi password, device token, bot token, khóa riêng tư SSH hoặc `.env` production vào repository, image hay log.
- ACK chỉ xác nhận event và outbox đã commit; không xác nhận Telegram đã nhận. Không hứa exactly-once cho Telegram.
- Một thông báo cho mỗi chuyển trạng thái; heartbeat không tạo spam. Có thể nhắc ALARM tối đa mỗi 5 phút khi sự cố còn active.
- Dùng server receive time để đánh giá liveness; không tin đồng hồ ESP32 để xác định thứ tự.
- Chưa gửi Telegram thật, nạp firmware, thay firewall hoặc kết nối VPS trước khi người dùng cho phép.
- `C:\Users\Admin\.ssh` chỉ là cấu hình SSH trên máy local, không phải thư mục runtime của server; không đọc khóa riêng tư.
- Chưa có repository Git và `C:\CANHBAOTHIENTAI` hiện rỗng; chỉ init/commit khi người dùng yêu cầu.

---

## File Structure

Tất cả đường dẫn dưới đây tương đối với `C:\CANHBAOTHIENTAI`; đây là các file mới.

- `docs/spec.md`: hợp đồng event/API, trạng thái báo động, retry, retention và tiêu chí nghiệm thu.
- `docs/hardware.md`: model, datasheet, sơ đồ chân/nguồn đã xác minh và kết quả hiệu chuẩn; không ghi giả định.
- `docs/runbook.md`: vận hành, cấp/thu hồi credential, backup/restore, thay chứng chỉ và xử lý sự cố.
- `firmware/platformio.ini`: board/toolchain đã chốt sau kiểm kê phần cứng.
- `firmware/include/config.example.h`: tên miền, CA, GPIO và ngưỡng dạng mẫu; không chứa secret thật.
- `firmware/include/alarm_logic.h`, `firmware/src/alarm_logic.cpp`: state machine thuần C++, không phụ thuộc ESP32.
- `firmware/include/event.h`, `firmware/src/event.cpp`: kiểu event, sequence và JSON serialization.
- `firmware/src/sensors.cpp`, `firmware/src/buzzer.cpp`, `firmware/src/main.cpp`: driver theo datasheet, còi, lịch đọc và mạng.
- `firmware/src/event_queue.cpp`, `firmware/src/transport.cpp`: hàng đợi bền vững, HTTPS CA validation, timeout và backoff.
- `firmware/test/test_alarm_logic.cpp`, `firmware/test/test_event.cpp`, `firmware/test/test_queue.cpp`: test native cho logic và hợp đồng.
- `backend/pyproject.toml`, `backend/app/config.py`, `backend/app/models.py`: dependencies, settings và schema v1.
- `backend/app/db.py`, `backend/app/ingest.py`, `backend/app/main.py`: SQLite transaction, ingest idempotent, FastAPI routes.
- `backend/app/notifications.py`, `backend/app/worker.py`, `backend/app/monitor.py`: outbox, Telegram retry, offline/recovery và reminder.
- `backend/tests/test_contract.py`, `test_ingest.py`, `test_worker.py`, `test_monitor.py`: test API, transaction, retry và liveness.
- `deploy/Dockerfile`, `deploy/compose.yaml`, `deploy/Caddyfile`, `deploy/.env.example`: image non-root, API/worker/proxy, volume và mẫu secrets.
- `scripts/simulate_device.py`, `scripts/backup_db.py`, `.gitignore`, `README.md`: giả lập E2E, backup nhất quán, loại secret và hướng dẫn.

Không có function hoặc utility nội bộ hiện hữu để tái sử dụng; dùng thư viện chuẩn và dependencies trên, không thêm MQTT, dashboard hoặc OTA trong phiên bản đầu.

---

## Task 1: Chốt đặc tả và kiểm kê phần cứng

**Files:**
- Create: `docs/spec.md`
- Create: `docs/hardware.md`
- Create: `docs/runbook.md`
- Create: `README.md`

**Interfaces:**
- Produces event schema v1, state transitions, API contract và acceptance gates cho mọi task sau.
- Produces hardware manifest bắt buộc gồm `esp32_model`, `flame_sensor_model`, `temperature_sensor_model`, `buzzer_model`, `logic_voltage`, `supply_voltage`, `gpio_map`, `datasheet_urls`.

- [ ] **Step 1: Ghi kiểm kê phần cứng dạng fail-closed**

  Trong `docs/hardware.md`, tạo bảng có các trường trên và giá trị `UNVERIFIED` cho mục chưa biết. Ghi rõ không được đấu dây/nạp firmware khi còn `UNVERIFIED` ở điện áp, GPIO hoặc driver còi.

- [ ] **Step 2: Ghi hợp đồng event/API**

  Trong `docs/spec.md`, dùng payload chuẩn:

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

  Định nghĩa `event` là `heartbeat`, `alarm_started`, `alarm_cleared`, `sensor_fault`, `sensor_recovered`; `temperature_c` và `flame_detected` nullable khi sensor fault; key idempotency là `(device_id, boot_id, seq)`.

- [ ] **Step 3: Ghi state machine và các cổng an toàn**

  Định nghĩa `NORMAL → SUSPECT → ALARM` và `NORMAL ↔ SENSOR_FAULT`; ALARM latch không tự clear sau reboot, chỉ clear khi sensor hợp lệ dưới ngưỡng phục hồi trong thời gian ổn định. Ghi rõ ngưỡng `temp_warn_c`, `temp_alarm_c`, `temp_clear_c`, thời gian xác nhận và hysteresis phải lấy từ datasheet/thử nghiệm, không phải giá trị mặc định an toàn.

- [ ] **Step 4: Kiểm tra tài liệu**

  Run: `rg -n "UNVERIFIED|setInsecure|exactly-once|GPIO|Telegram" docs README.md`

  Expected: các mục phần cứng chưa xác minh hiển thị rõ; không có hướng dẫn tắt TLS; mọi ngưỡng đều được gắn nhãn cần hiệu chuẩn.

- [ ] **Step 5: Commit tài liệu khi repository đã được người dùng cho phép**

  ```bash
  git add docs README.md
  git commit -m "docs: define fire alarm system contract"
  ```

## Task 2: Implement state machine báo động cục bộ bằng TDD

**Files:**
- Create: `firmware/include/alarm_logic.h`
- Create: `firmware/src/alarm_logic.cpp`
- Create: `firmware/test/test_alarm_logic.cpp`
- Modify: `docs/spec.md`

**Interfaces:**
- Consumes: `SensorReading { bool flame_valid; bool flame_detected; bool temperature_valid; float temperature_c; bool hardware_fault; }`.
- Produces: `AlarmState { NORMAL, SUSPECT, ALARM, SENSOR_FAULT }` và `AlarmTransition { NONE, ALARM_STARTED, ALARM_CLEARED, SENSOR_FAULT_STARTED, SENSOR_FAULT_CLEARED }`.
- Function: `AlarmTransition AlarmLogic::update(const SensorReading& reading, uint32_t now_ms)`; `AlarmState AlarmLogic::state() const`; `bool AlarmLogic::latched() const`; `void AlarmLogic::acknowledgeLocalClear(uint32_t now_ms)`.
- Constructor: `AlarmLogic(AlarmConfig config)` với config gồm ngưỡng, `confirm_ms`, `clear_ms`, `sensor_fault_ms` và `hysteresis`.

- [ ] **Step 1: Viết test fail cho xác nhận lửa/nhiệt**

  ```cpp
  TEST_CASE("short flame pulse does not start alarm") {
      AlarmLogic logic(testConfig(1000));
      CHECK(logic.update(reading(true, 0, true, 25), 0) == NONE);
      CHECK(logic.update(reading(true, 0, true, 25), 999) == NONE);
      CHECK(logic.update(reading(false, 0, true, 25), 1000) == NONE);
  }

  TEST_CASE("confirmed flame starts latched alarm") {
      AlarmLogic logic(testConfig(1000));
      CHECK(logic.update(reading(true, 0, true, 25), 0) == NONE);
      CHECK(logic.update(reading(true, 0, true, 25), 1000) == ALARM_STARTED);
      CHECK(logic.state() == ALARM);
      CHECK(logic.latched());
  }
  ```

- [ ] **Step 2: Thêm test hysteresis, clear an toàn và sensor fault**

  Test nhiệt trên `temp_alarm_c` đủ `confirm_ms`, nhiệt dưới `temp_clear_c` chưa clear nếu chưa đủ `clear_ms`, lỗi một sensor không che sensor còn lại, lỗi cả hai phát `SENSOR_FAULT_STARTED`, và reboot/new object không tự clear latch được thể hiện qua persisted incident state trong caller.

- [ ] **Step 3: Chạy test fail**

  Run: `cd firmware; pio test -e native -f test_alarm_logic -v`

  Expected: FAIL vì header/implementation chưa tồn tại hoặc hành vi chưa được cài.

- [ ] **Step 4: Viết implementation tối thiểu**

  Implement chuyển trạng thái bằng `uint32_t` elapsed arithmetic, không dùng `delay()`. ALARM được kích bởi flame đã xác nhận hoặc nhiệt đã xác nhận; clear chỉ khi điều kiện phục hồi hợp lệ và `acknowledgeLocalClear()` được gọi.

- [ ] **Step 5: Chạy test pass và cập nhật spec**

  Run: `cd firmware; pio test -e native -f test_alarm_logic -v`

  Expected: PASS toàn bộ test state machine; cập nhật `docs/spec.md` để tên enum và thời gian khớp header.

- [ ] **Step 6: Commit**

  ```bash
  git add firmware/include/alarm_logic.h firmware/src/alarm_logic.cpp firmware/test/test_alarm_logic.cpp docs/spec.md
  git commit -m "feat: add local fire alarm state machine"
  ```

## Task 3: Tạo event contract và hàng đợi offline

**Files:**
- Create: `firmware/include/event.h`
- Create: `firmware/src/event.cpp`
- Create: `firmware/src/event_queue.cpp`
- Create: `firmware/test/test_event.cpp`
- Create: `firmware/test/test_queue.cpp`

**Interfaces:**
- Consumes: `AlarmTransition` và `SensorReading` từ Task 2.
- Produces: `TelemetryEvent makeEvent(const DeviceSnapshot&, const SensorReading&, AlarmTransition)`; `std::string serializeJson(const TelemetryEvent&)`; `bool EventQueue::push(const TelemetryEvent&)`; `bool EventQueue::pop(TelemetryEvent&)`; `size_t EventQueue::size() const`.
- `TelemetryEvent` chứa đúng các key schema v1; `boot_id` tăng một lần trong NVS mỗi boot; `seq` tăng đơn điệu trong boot; `incident_id` giữ nguyên trong ALARM.

- [ ] **Step 1: Viết test serialization và sequence**

  ```cpp
  TEST_CASE("event serialization preserves nullable sensor values") {
      auto event = makeFaultEvent("kitchen-01", "boot-1", 7);
      auto json = serializeJson(event);
      CHECK(json.find("\"temperature_c\":null") != std::string::npos);
      CHECK(json.find("\"sensor_fault\":true") != std::string::npos);
  }

  TEST_CASE("event key is unique per boot and sequence") {
      CHECK(eventKey(event("d", "b1", 1)) != eventKey(event("d", "b1", 2)));
      CHECK(eventKey(event("d", "b1", 1)) != eventKey(event("d", "b2", 1)));
  }
  ```

- [ ] **Step 2: Viết test queue policy**

  Test event chuyển trạng thái được giữ khi queue đầy, heartbeat bị gộp/bỏ theo policy đã ghi trong spec, FIFO cho event quan trọng, và restart load lại event chưa gửi.

- [ ] **Step 3: Chạy test fail**

  Run: `cd firmware; pio test -e native -f test_event -f test_queue -v`

  Expected: FAIL do contract chưa có.

- [ ] **Step 4: Implement JSON và queue**

  Dùng ArduinoJson hoặc serializer đã pin trong `platformio.ini`; dùng NVS/Preferences cho boot counter, queue bền vững giới hạn kích thước, không ghi flash mỗi heartbeat. Khi đầy, ưu tiên `alarm_started`, `alarm_cleared`, `sensor_fault`; ghi counter `dropped_heartbeats` vào event kế tiếp.

- [ ] **Step 5: Chạy test pass**

  Run: `cd firmware; pio test -e native -f test_event -f test_queue -v`

  Expected: PASS; output JSON parse được bằng parser backend ở Task 5.

- [ ] **Step 6: Commit**

  ```bash
  git add firmware/include/event.h firmware/src/event.cpp firmware/src/event_queue.cpp firmware/test/test_event.cpp firmware/test/test_queue.cpp
  git commit -m "feat: add durable device event queue"
  ```

## Task 4: Tích hợp driver ESP32, còi và HTTPS transport

**Files:**
- Create: `firmware/platformio.ini`
- Create: `firmware/include/config.example.h`
- Create: `firmware/src/sensors.cpp`
- Create: `firmware/src/buzzer.cpp`
- Create: `firmware/src/transport.cpp`
- Create: `firmware/src/main.cpp`
- Modify: `firmware/test/test_queue.cpp`
- Modify: `docs/hardware.md`, `docs/runbook.md`

**Interfaces:**
- Consumes: `AlarmLogic`, `TelemetryEvent`, `EventQueue` từ Tasks 2-3.
- Produces: `SensorReading readSensors()`, `void setBuzzer(bool on)`, `TransportResult postEvent(const TelemetryEvent&)`.
- `TransportResult` là `SENT`, `DUPLICATE_ACK`, `RETRYABLE_FAILURE`, `PERMANENT_FAILURE`; transport không được gọi `setInsecure()`.

- [ ] **Step 1: Hoàn tất hardware gate trước code board**

  Điền model/datasheet/GPIO/voltage thực tế vào `docs/hardware.md`; chụp lại phép đo mức logic và dòng còi. Nếu thiếu bất kỳ mục nào, chỉ chạy native tests và không bật `board = esp32` build.

- [ ] **Step 2: Viết test transport contract bằng mock**

  Mock HTTP trả 200 accepted, 200 duplicate, 401, 422, 429 và timeout; kiểm tra mapping vào `TransportResult`, queue không mất event khi retryable failure, và permanent failure không retry vô hạn.

- [ ] **Step 3: Chạy test fail**

  Run: `cd firmware; pio test -e native -f test_queue -v`

  Expected: FAIL ở các mock transport/queue assertions mới.

- [ ] **Step 4: Implement vòng lặp không chặn**

  `main.cpp` đọc sensor/còi theo tick độc lập mạng; transport dùng `WiFiClientSecure` với CA certificate pin/configured, timeout hữu hạn, exponential backoff có jitter. Còi bật theo `AlarmLogic` ngay cả khi Wi-Fi down. Không log token/header bí mật.

- [ ] **Step 5: Build và test**

  Set `VERIFIED_PLATFORMIO_ENV` to the environment name recorded in `firmware/platformio.ini` after the hardware gate, then run: `cd firmware; pio test -e native -v`; `pio run -e "$VERIFIED_PLATFORMIO_ENV"`.

  Expected: native tests PASS; board build PASS; khi ngắt Wi-Fi, còi vẫn phản hồi state machine và event còn trong queue.

- [ ] **Step 6: Commit**

  ```bash
  git add firmware docs/hardware.md docs/runbook.md
  git commit -m "feat: integrate ESP32 sensors buzzer and HTTPS transport"
  ```

## Task 5: Xây backend schema, SQLite transaction và ingest API

**Files:**
- Create: `backend/pyproject.toml`
- Create: `backend/app/config.py`
- Create: `backend/app/models.py`
- Create: `backend/app/db.py`
- Create: `backend/app/ingest.py`
- Create: `backend/app/main.py`
- Create: `backend/tests/test_contract.py`
- Create: `backend/tests/test_ingest.py`

**Interfaces:**
- Consumes: JSON event v1 từ Task 3/4 và per-device bearer token từ environment/secret store.
- Produces: `POST /api/v1/telemetry` với `200 {"accepted": true, "duplicate": false|true}`; `GET /healthz` không lộ secret; `ingest_event(db, event, device) -> IngestResult`.
- SQLite unique key `(device_id, boot_id, seq)`; bảng `events`, `device_state`, `outbox` cùng transaction.

- [ ] **Step 1: Viết test contract và status codes**

  ```python
  def test_valid_event_is_accepted_and_creates_outbox(client):
      response = client.post("/api/v1/telemetry", headers=auth(), json=alarm_started())
      assert response.status_code == 200
      assert response.json() == {"accepted": True, "duplicate": False}
      assert count_events() == 1
      assert count_outbox() == 1

  def test_same_event_is_idempotent(client):
      client.post("/api/v1/telemetry", headers=auth(), json=alarm_started())
      response = client.post("/api/v1/telemetry", headers=auth(), json=alarm_started())
      assert response.json()["duplicate"] is True
      assert count_outbox() == 1
  ```

  Thêm test 401/403 token sai, 422 schema sai, 409 cùng key nhưng payload hash khác, 503 khi commit lỗi, event cũ không làm lùi `device_state`.

- [ ] **Step 2: Chạy test fail**

  Run: `cd backend; python -m pytest tests/test_contract.py tests/test_ingest.py -q`

  Expected: FAIL vì package/app/routes chưa tồn tại.

- [ ] **Step 3: Implement models và transaction**

  Pydantic giới hạn schema/body 8 KiB; `ingest_event()` bắt đầu transaction, insert event, cập nhật state nếu `(boot_id, seq)` mới hơn, tạo notification key duy nhất cho `alarm_started`, `alarm_cleared`, fault/recovery, rồi commit trước response.

- [ ] **Step 4: Implement auth và routes**

  Map `device_id -> argon2/bcrypt hash` hoặc secret do environment cung cấp; dùng constant-time compare; không ghi credential vào log. `/healthz` kiểm tra process và DB read/write nhẹ, trả 503 khi DB không dùng được.

- [ ] **Step 5: Chạy test pass**

  Run: `cd backend; python -m pytest tests/test_contract.py tests/test_ingest.py -q`

  Expected: PASS; test transaction rollback xác nhận không có ACK thành công khi insert/outbox commit thất bại.

- [ ] **Step 6: Commit**

  ```bash
  git add backend
  git commit -m "feat: add transactional telemetry ingest API"
  ```

## Task 6: Implement Telegram outbox, worker và offline monitor

**Files:**
- Create: `backend/app/notifications.py`
- Create: `backend/app/worker.py`
- Create: `backend/app/monitor.py`
- Create: `backend/tests/test_worker.py`
- Create: `backend/tests/test_monitor.py`
- Modify: `backend/app/db.py`, `backend/app/ingest.py`

**Interfaces:**
- Consumes pending rows từ `outbox` của Task 5.
- Produces: `claim_outbox(now, lease_seconds)`, `send_telegram(row)`, `run_once(now)`, `monitor_liveness(now)`.
- Outbox fields: `notification_key UNIQUE`, `payload`, `status`, `attempts`, `next_attempt_at`, `lease_until`, `last_error`, `sent_at`.

- [ ] **Step 1: Viết test retry/idempotency**

  ```python
  def test_timeout_keeps_outbox_pending_with_backoff(fake_telegram):
      fake_telegram.timeout = True
      run_once(now=t0)
      row = get_outbox("device:incident:alarm_started")
      assert row.status == "pending"
      assert row.attempts == 1
      assert row.next_attempt_at > t0

  def test_worker_restart_claims_expired_lease_without_losing_event():
      claim_outbox(t0, lease_seconds=30)
      assert claim_outbox(t0 + timedelta(seconds=31), lease_seconds=30)
  ```

  Thêm test HTTP 429 dùng `retry_after`, 5xx/network retry, 400/401/403 chuyển `failed`, Telegram success đánh dấu `sent`, crash window cho phép duplicate nhưng không tạo outbox row thứ hai.

- [ ] **Step 2: Viết test offline/recovery**

  Với heartbeat mới nhất quá 60 giây, tạo một `offline` key; các lần monitor tiếp theo không tạo thêm row; khi nhận telemetry tươi tạo một `recovery` key đúng một lần.

- [ ] **Step 3: Chạy test fail**

  Run: `cd backend; python -m pytest tests/test_worker.py tests/test_monitor.py -q`

  Expected: FAIL vì worker/monitor chưa có.

- [ ] **Step 4: Implement worker**

  Claim nguyên tử bằng transaction/lease; timeout HTTP hữu hạn; exponential backoff có jitter và giới hạn; xử lý `Retry-After`; chỉ `sent` sau response Telegram thành công; không retry vô hạn lỗi credential.

- [ ] **Step 5: Implement message formatter và monitor**

  Message gồm vị trí, device, loại event, server receive time, nhiệt độ nếu hợp lệ, sensor fault và `incident_id`; không gửi mỗi heartbeat. Nhắc ALARM không quá 5 phút; liveness dùng server time.

- [ ] **Step 6: Chạy test pass và commit**

  Run: `cd backend; python -m pytest -q`

  Expected: PASS toàn bộ backend tests.

  ```bash
  git add backend
  git commit -m "feat: add durable Telegram notification worker"
  ```

## Task 7: Đóng gói Docker, HTTPS và secrets

**Files:**
- Create: `deploy/Dockerfile`
- Create: `deploy/compose.yaml`
- Create: `deploy/Caddyfile`
- Create: `deploy/.env.example`
- Modify: `.gitignore`, `README.md`, `docs/runbook.md`

**Interfaces:**
- Caddy nhận HTTPS ở giá trị `PUBLIC_DOMAIN` đã xác minh, proxy `/api/*` tới API nội bộ; DB không publish port.
- `api`, `worker`, `caddy` dùng volume SQLite riêng; API và worker dùng cùng image/code nhưng command khác.
- Required env: `PUBLIC_DOMAIN`, `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID`, `DEVICE_CREDENTIALS_FILE`, `DATABASE_PATH`.

- [ ] **Step 1: Viết config validation test**

  Test production startup fail nếu thiếu bot token, chat ID, device credential hoặc domain; test `.env.example` chỉ có placeholder và không có token thật.

- [ ] **Step 2: Tạo image non-root và healthcheck**

  Pin base image/dependencies, chạy user không phải root, `read_only` nơi phù hợp, volume `/data`, restart policy và healthcheck gọi `/healthz`.

- [ ] **Step 3: Viết Caddy và Compose**

  Chỉ expose `80/443` từ Caddy; API/worker dùng internal network; Caddy tự cấp/renew certificate cho domain đã có DNS. Firmware giữ CA validation, không dùng HTTP fallback.

- [ ] **Step 4: Chạy local integration**

  Run: `docker compose -f deploy/compose.yaml config`; Expected: PASS và không có hard-coded secret.

  Run: `docker compose -f deploy/compose.yaml up -d`; Expected: `curl -fsS http://127.0.0.1/healthz` chỉ dùng local smoke nếu route được expose tạm thời; không coi đây là HTTPS acceptance.

- [ ] **Step 5: Commit**

  ```bash
  git add deploy .gitignore README.md docs/runbook.md
  git commit -m "ops: package API worker and HTTPS proxy"
  ```

## Task 8: E2E simulator, backup/restore và nghiệm thu VPS

**Files:**
- Create: `scripts/simulate_device.py`
- Create: `scripts/backup_db.py`
- Create: `backend/tests/test_e2e.py`
- Modify: `docs/runbook.md`, `docs/hardware.md`, `README.md`

**Interfaces:**
- `simulate_device.py --base-url URL --device-id ID --credential-file PATH --scenario alarm-cycle` gửi alarm start, duplicate, stale event, clear, offline/recovery mà không cần phần cứng.
- `backup_db.py --database PATH --output PATH` dùng SQLite backup API và kiểm tra integrity.

- [ ] **Step 1: Viết E2E test fail**

  Kịch bản phải assert: alarm cycle tạo một Telegram outbox duy nhất; duplicate không tạo thêm; stale clear không xóa alarm mới; worker retry tiếp tục sau restart; offline/recovery mỗi loại một notification.

- [ ] **Step 2: Chạy test fail**

  Run: `cd backend; python -m pytest tests/test_e2e.py -q`

  Expected: FAIL trước khi simulator và Compose integration tồn tại.

- [ ] **Step 3: Implement simulator và backup**

  Simulator dùng cùng JSON schema/credential path như firmware; backup gọi `sqlite3.Connection.backup()` sang file tạm, chạy `PRAGMA integrity_check`, rồi rename atomically. Không copy trực tiếp file WAL đang ghi.

- [ ] **Step 4: Chạy E2E pass**

  Set `VERIFIED_BASE_URL` to `https://` plus the domain recorded in `PUBLIC_DOMAIN`, and set `DEVICE_CREDENTIAL_FILE` to the protected credential file path on the authorized host. Run: `docker compose -f deploy/compose.yaml up -d`; `python scripts/simulate_device.py --base-url "$VERIFIED_BASE_URL" --device-id kitchen-01 --credential-file "$DEVICE_CREDENTIAL_FILE" --scenario alarm-cycle`; `cd backend; python -m pytest -q`.

  Expected: API ACK đúng, outbox xử lý được, Telegram thật chỉ dùng khi người dùng đã cấp phép; test phải có fake Telegram cho CI.

- [ ] **Step 5: Test backup/restore và reboot**

  Run backup trên VPS, restore vào DB test riêng, chạy `PRAGMA integrity_check`, xác nhận `events`, `device_state`, pending outbox còn nguyên; reboot VPS rồi xác nhận container restart và outbox tiếp tục.

- [ ] **Step 6: Ghi acceptance checklist**

  `docs/runbook.md` phải có: DNS/HTTPS hợp lệ, firewall chỉ mở 22/80/443 theo chính sách, DB không public, TLS firmware xác thực CA, còi hoạt động khi tắt Wi-Fi, reboot không mất latch/outbox, credential rotation, backup ngoài VPS và restore thử.

- [ ] **Step 7: Commit khi người dùng yêu cầu**

  ```bash
  git add scripts backend/tests docs README.md
  git commit -m "test: verify device to Telegram alarm flow"
  ```

---

## Self-Review

- **Spec coverage:** Hardware/voltage/GPIO gate Task 1 and Task 4; local alarm, latch, hysteresis and sensor fault Task 2; event ordering and offline queue Task 3; CA-validated HTTPS and local buzzer Task 4; auth, payload limits, idempotency and transactional ACK Task 5; Telegram retry, duplicate tolerance, offline/recovery and reminders Task 6; Docker, HTTPS, non-root and secret boundaries Task 7; E2E, backup/restore and reboot acceptance Task 8.
- **Placeholder scan:** No unspecified implementation step remains. Unknown hardware is represented as `UNVERIFIED` with an explicit gate; `VERIFIED_PLATFORMIO_ENV`, `PUBLIC_DOMAIN`, `VERIFIED_BASE_URL` and `DEVICE_CREDENTIAL_FILE` are named configuration inputs with defined sources, not hidden implementation placeholders.
- **Type consistency:** `TelemetryEvent`, `AlarmTransition`, `AlarmState`, `TransportResult`, `POST /api/v1/telemetry`, outbox fields and simulator command are defined before later tasks consume them.
- **Known residual risk:** Lửa+nhiệt không thay thế đầu báo khói đạt chuẩn; mất điện làm mất khả năng phát hiện nếu không có UPS/battery; Telegram delivery is at-least-once. These are acceptance constraints, not software bugs.

## Approval Gate

Sau khi plan được duyệt, thực thi theo từng task và dừng tại các cổng: hardware manifest đã xác minh, native firmware tests PASS, backend tests PASS, Compose config PASS, E2E fake-HTTP PASS, rồi mới xin phép dùng VPS/Telegram thật. Không nạp firmware hoặc sửa VPS trong lúc chỉ lập kế hoạch.
