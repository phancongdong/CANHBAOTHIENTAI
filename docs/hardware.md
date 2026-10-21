# Bảng Kiểm Kê Phần Cứng (Hardware Manifest - Fail-Closed)

> **CẢNH BÁO QUAN TRỌNG (SAFETY GATE):**
> Tuyệt đối KHÔNG đấu dây, cấp nguồn hoặc nạp firmware cho ESP32 khi bất kỳ mục nào về điện áp (`logic_voltage`, `supply_voltage`), `gpio_map`, hoặc driver còi (`buzzer_model`) còn ở trạng thái `UNVERIFIED`.
> ESP32 hoạt động ở mức logic 3.3V. Đưa tín hiệu 5V vào GPIO của ESP32 sẽ gây hỏng vi điều khiển vĩnh viễn. Không nối còi tải lớn trực tiếp vào GPIO mà phải qua mạch đệm transistor/MOSFET.

## 1. Trạng Thái Xác Minh Linh Kiện

| Thuộc tính | Giá trị hiện tại | Ghi chú & Yêu cầu xác minh |
| :--- | :--- | :--- |
| `esp32_model` | `UNVERIFIED` | Cần xác minh: ESP32-WROOM-32, NodeMCU ESP32, ESP32-S3, v.v. |
| `flame_sensor_model` | `UNVERIFIED` | Ví dụ: YG1006 / Cảm biến lửa hồng ngoại 760nm - 1100nm (DO/AO). Cần bộ chuyển đổi mức logic nếu module chạy 5V. |
| `temperature_sensor_model` | `UNVERIFIED` | Ví dụ: DS18B20 (1-Wire, cần trở kéo 4.7k) hoặc DHT22/SHT3x (I2C/Single-bus). |
| `buzzer_model` | `UNVERIFIED` | Còi chủ động (Active Buzzer) 5V/3.3V hoặc còi thụ động (Passive Buzzer). Cần transistor lái (2N2222/SS8050) và diode bảo vệ chống xung ngược. |
| `logic_voltage` | `UNVERIFIED` | Mức logic GPIO ESP32 là 3.3V. Logic của cảm biến phải tương thích 3.3V hoặc qua Level Shifter. |
| `supply_voltage` | `UNVERIFIED` | Nguồn cấp hệ thống: 5V qua cổng microUSB/Type-C hoặc nguồn xung 5V/2A riêng biệt. |
| `gpio_map` | `UNVERIFIED` | Danh sách gán chân GPIO chính thức sau khi chốt phần cứng. Tuyệt đối không dùng các chân strapping (GPIO0, GPIO2, GPIO12, GPIO15) làm chân kích còi mặc định. |
| `datasheet_urls` | `UNVERIFIED` | Link datasheet chính thức của từng linh kiện. |

---

## 2. Dự Kiến Sơ Đồ Chân (Chờ Xác Minh Thực Tế)

*Lưu ý: Bảng này chỉ là thiết kế tham chiếu ban đầu, cần kiểm tra với board thực tế trước khi cấu hình firmware.*

| Thiết bị | Chân đề xuất (ESP32) | Chế độ | Điện áp hoạt động | Biện pháp bảo vệ |
| :--- | :--- | :--- | :--- | :--- |
| Flame Sensor DO | GPIO 4 | Digital INPUT | 3.3V | Phân áp trở nếu module cấp 5V |
| Temp Sensor (DS18B20) | GPIO 18 | 1-Wire BIDIR | 3.3V | Điện trở kéo 4.7kΩ lên 3.3V |
| Buzzer Driver | GPIO 19 | Digital OUTPUT | 3.3V logic -> 5V còi | Transistor NPN (S8050) + diode 1N4148 |
| Nút Reset Báo Động Tại Chỗ | GPIO 0 / Nút nhấn | Digital INPUT_PULLUP | 3.3V | Chống rung phần mềm (debounce 50ms) |

---

## 3. Quy Trình Cổng An Toàn (Hardware Gate Protocol)

1. **Bước 1:** Chụp ảnh board ESP32, cảm biến lửa, cảm biến nhiệt và còi gửi kèm thông số khắc trên thân linh kiện.
2. **Bước 2:** Đo điện áp cấp và điện áp đầu ra tín hiệu (Vout) bằng đồng hồ vạn năng (VOM).
3. **Bước 3:** Cập nhật file này từ `UNVERIFIED` thành mã linh kiện cụ thể và link datasheet.
4. **Bước 4:** Chỉ mở khóa nạp firmware trên thiết bị thực tế sau khi toàn bộ bảng trên đã được xác thực `VERIFIED`.
