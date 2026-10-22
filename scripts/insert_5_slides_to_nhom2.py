import pptx
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
import shutil, os

SRC_PATH = r"C:\Users\Admin\Downloads\Nhom2.pptx"
BAK_PATH = r"C:\Users\Admin\Downloads\Nhom2.pptx.original_bak"
OUT_DESKTOP = r"C:\Users\Admin\Desktop\Nhom2_Them_5_Slide_Workflow_Tele.pptx"
OUT_DOWNLOADS = r"C:\Users\Admin\Downloads\Nhom2_Them_5_Slide_Workflow_Tele.pptx"
OUT_DOCS = r"C:\CANHBAOTHIENTAI\docs\Nhom2_Them_5_Slide_Workflow_Tele.pptx"

# Sao lưu nguyên bản file gốc để bảo vệ tuyệt đối
if not os.path.exists(BAK_PATH):
    shutil.copy2(SRC_PATH, BAK_PATH)
    print(f"Da tao ban sao luu an toan tai: {BAK_PATH}")

prs = pptx.Presentation(SRC_PATH)
original_slide_count = len(prs.slides)
print(f"File goc Nhom2.pptx co {original_slide_count} slides.")

# Cấu hình màu sắc phù hợp nhận diện của Nhom2.pptx
COLOR_NAVY_TITLE = RGBColor(30, 60, 110)   # #1E3C6E
COLOR_NAVY_SUB   = RGBColor(0, 86, 145)    # #005691
COLOR_TEXT_MAIN  = RGBColor(40, 50, 65)    # #283241
COLOR_CARD_BG    = RGBColor(248, 250, 252) # #F8FAFC
COLOR_BORDER     = RGBColor(220, 226, 235) # #DCE2EB

slides_data = [
    {
        "category": "MỞ RỘNG ĐỀ TÀI - HỆ THỐNG CẢNH BÁO THIÊN TAI IOT",
        "title": "TỔNG QUAN KIẾN TRÚC WORKFLOW GỬI CẢNH BÁO TELEGRAM",
        "card_left": {
            "title": "1. KIẾN TRÚC 4 TẦNG & LUỒNG TÍN HIỆU ĐẦU CUỐI (E2E)",
            "bullets": [
                ("● Tầng Thiết bị Biên (ESP32): ", "Quét cảm biến MPU-6050 (rung chấn), Flame AO (quang trở lửa), DHT11 (nhiệt độ). Kích hoạt còi/LED 10Hz tại chỗ tức thì khi chạm ngưỡng."),
                ("● Tầng Truyền dẫn Mạng: ", "Đóng gói JSON kèm Bearer Token bảo mật, truyền qua Wi-Fi 2.4GHz với giao thức HTTP POST đến cổng 80 của VPS 103.178.235.184."),
                ("● Tầng Máy chủ Đám mây (VPS): ", "FastAPI tiếp nhận, xác thực Token, ghi cơ sở dữ liệu SQLite telemetry, lọc chống bão tin (Cooldown) và đẩy vào hàng đợi alert_outbox."),
                ("● Tầng Phân phối Tin nhắn: ", "Tiến trình Worker quét hàng đợi ngầm, gọi Telegram Bot API gửi thông báo đẩy (Push Notification) tức thời đến smartphone người dùng.")
            ]
        },
        "card_right": {
            "title": "2. GIÁ TRỊ KỸ THUẬT & ƯU THẾ VƯỢT TRỘI",
            "bullets": [
                ("● Khắc phục giới hạn còi tại chỗ: ", "Người dùng nhận cảnh báo tức thời ngay cả khi vắng nhà, đi công tác xa hoặc đang ngủ say."),
                ("● Độ trễ toàn trình cực thấp (0.8s - 1.5s): ", "Từ lúc cảm biến rung lắc/bắt lửa đến khi chuông điện thoại reo chỉ mất khoảng 1 giây."),
                ("● Tính độc lập & An toàn sinh mạng: ", "Nếu mất kết nối Internet, còi và đèn tại chỗ vẫn hú 10Hz bình thường, bảo vệ an toàn 100%."),
                ("● Đa nền tảng, không cần cài App phụ: ", "Nhận thông báo trực tiếp qua ứng dụng Telegram trên iOS, Android, Windows, macOS và Smartwatch.")
            ]
        }
    },
    {
        "category": "MỞ RỘNG ĐỀ TÀI - TẦNG PHẦN CỨNG & XỬ LÝ BIÊN (EDGE LAYER)",
        "title": "TẦNG BIÊN ESP32: CƠ CHẾ ĐA NHÂN FREERTOS & BÁO ĐỘNG TỨC THÌ",
        "card_left": {
            "title": "1. ĐỘT PHÁ ĐA NHÂN (DUAL-CORE) - LOẠI BỎ ĐỘ TRỄ MẠNG",
            "bullets": [
                ("● Vấn đề nghẽn mạng ở thiết kế cũ: ", "Hàm gửi HTTPS/SSL đến Telegram mất 2 - 4 giây chặn luồng (blocking) khiến còi bị 'câm' hoặc đơ nhịp khi có động đất."),
                ("● Phân bổ tác vụ trên Core 1 (Real-time): ", "Quét cảm biến mỗi 50ms. Khi phát hiện rung chấn >0.5g hoặc lửa -> Kích hoạt Còi & LED 10Hz NGAY LẬP TỨC (0ms)."),
                ("● Tác vụ mạng ngầm trên Core 0 (NetWorker): ", "Âm thầm xử lý SSL và đẩy dữ liệu lên Cloud -> Còi tại Core 1 hú dồn dập 10Hz mượt mà, không gián đoạn."),
                ("● Tối ưu chu kỳ đọc DHT11 (1.5s): ", "Tránh lỗi tràn bộ đệm đọc I2C và cảm biến nhiệt, bảo đảm độ ổn định cho vòng lặp điều khiển.")
            ]
        },
        "card_right": {
            "title": "2. PHÁT HIỆN CẢM BIẾN OFFLINE & ĐÓNG GÓI GÓI TIN",
            "bullets": [
                ("● Giám sát bus I2C thời gian thực: ", "Hàm isAccelOnline() kiểm tra đường truyền SDA (21) / SCL (22) liên tục mỗi chu kỳ."),
                ("● Báo động sự cố tuột dây tức thì: ", "Nếu MPU-6050 bị lỏng dây hoặc Offline -> Còi hú 10Hz khẩn cấp tại chỗ, LCD in 'LOI PHAN CUNG! CAM BIEN OFFLINE'."),
                ("● Gói tin JSON chuẩn hóa công nghiệp: ", "Đóng gói device_id, chuỗi thứ tự seq, gia tốc vibration_g, chỉ số lửa flame_raw, nhiệt độ temperature_c."),
                ("● Xác thực Bearer Token: ", "Gắn mã định danh bí mật trong HTTP Header để máy chủ VPS phân quyền và từ chối các thiết bị lạ.")
            ]
        }
    },
    {
        "category": "MỞ RỘNG ĐỀ TÀI - TẦNG DỊCH VỤ ĐÁM MÂY (BACKEND CLOUD)",
        "title": "TẦNG MÁY CHỦ VPS: TIẾP NHẬN, XÁC THỰC & THUẬT TOÁN COOLDOWN",
        "card_left": {
            "title": "1. DỊCH VỤ FASTAPI BACKEND & BẢO MẬT XÁC THỰC",
            "bullets": [
                ("● Kiến trúc Microservices chạy trên VPS: ", "Vận hành trên Ubuntu Server độc lập (103.178.235.184:80), quản lý dịch vụ tự phục hồi qua Linux Systemd."),
                ("● Kiểm thực Token bảo mật (Auth Check): ", "Mọi gói tin gửi lên đều được so khớp với danh bạ device_credentials.json, từ chối ngay 401 Unauthorized nếu sai lệch."),
                ("● Lưu trữ cơ sở dữ liệu SQLite toàn vẹn: ", "Lưu lịch sử chỉ số cảm biến vào bảng telemetry với tem thời gian UTC phục vụ trích xuất nhật ký và vẽ đồ thị."),
                ("● Phản hồi siêu tốc (Non-blocking): ", "FastAPI tiếp nhận và trả mã 200 OK cho ESP32 trong vòng 15ms để giải phóng tài nguyên vi điều khiển.")
            ]
        },
        "card_right": {
            "title": "2. THUẬT TOÁN COOLDOWN CHỐNG BÃO TIN NHẮN (ALERT STORM)",
            "bullets": [
                ("● Nguy cơ bão tin nhắn khi xảy ra sự cố: ", "Khi rung chấn hoặc hỏa hoạn kéo dài 1-2 phút, nếu mỗi 100ms gửi 1 tin -> Telegram chặn spam và người dùng hoảng loạn."),
                ("● Cơ chế Cooldown 60 giây thông minh: ", "Khi có báo động đỏ, VPS tạo 1 bản ghi PENDING gửi Telegram; các chu kỳ tiếp theo trong 60s chỉ cập nhật dữ liệu nền."),
                ("● Cơ chế tự phục hồi (Recovery Notification): ", "Khi chỉ số cảm biến hạ dưới ngưỡng nguy hiểm, hệ thống tự động gửi 1 tin báo 'Hệ thống an toàn trở lại'."),
                ("● Phân loại đa cấp độ cảnh báo: ", "Tự động phân cấp: Báo động Đỏ (10Hz), Cảnh báo sớm (1Hz), Báo lỗi cảm biến và Trạng thái an toàn.")
            ]
        }
    },
    {
        "category": "MỞ RỘNG ĐỀ TÀI - TẦNG BẢO ĐẢM TRUYỀN THÔNG TIN CẬY",
        "title": "MÔ HÌNH TRANSACTIONAL OUTBOX & KHẢ NĂNG CHỊU LỖI WORKER",
        "card_left": {
            "title": "1. THIẾT KẾ MÔ HÌNH TRANSACTIONAL OUTBOX PATTERN",
            "bullets": [
                ("● Tách biệt hoàn toàn API và Gửi tin: ", "Luồng nhận tin (FastAPI) không gọi trực tiếp Telegram API mà ủy quyền cho tiến trình ngầm canhbaothientai-worker."),
                ("● Hàng đợi cơ sở dữ liệu bền vững (Outbox): ", "Các cảnh báo được ghi vào bảng alert_outbox với trạng thái ban đầu PENDING."),
                ("● Bảo đảm tính toàn vẹn (ACID Transaction): ", "Không bao giờ bị mất tin báo ngay cả khi server mất điện đột ngột hoặc tiến trình bị khởi động lại."),
                ("● Tiến trình Worker xử lý liên tục: ", "Worker quét bảng hàng đợi mỗi 500ms, lấy các bản ghi PENDING để định dạng tin nhắn HTML và gọi Telegram.")
            ]
        },
        "card_right": {
            "title": "2. CƠ CHẾ CHỊU LỖI & THỬ LẠI TỰ ĐỘNG (EXPONENTIAL BACKOFF)",
            "bullets": [
                ("● Ứng phó sự cố đứt cáp hoặc Telegram nghẽn: ", "Nếu kết nối đến api.telegram.org gặp sự cố (Timeout, HTTP 429, 502/503), tin nhắn không bị hủy."),
                ("● Thuật toán thử lại theo hàm mũ: ", "Tăng dần retry_count và giãn cách thời gian thử lại (2s, 4s, 8s, 16s...) bảo đảm hệ thống không bị quá tải."),
                ("● Cam kết chuyển phát (At-Least-Once Delivery): ", "Cảnh báo chỉ được đánh dấu SENT khi máy chủ Telegram chính thức trả về mã HTTP 200 OK."),
                ("● Lưu vết kiểm toán (Audit Trail): ", "Ghi nhận chính xác mốc thời gian sent_at và retry_count phục vụ hậu kiểm và chẩn đoán đường truyền.")
            ]
        }
    },
    {
        "category": "MỞ RỘNG ĐỀ TÀI - GIAO DIỆN NGƯỜI DÙNG & ĐÁNH GIÁ THỰC NGHIỆM",
        "title": "TRỰC QUAN HÓA THÔNG BÁO TELEGRAM & ĐÁNH GIÁ HIỆU NĂNG",
        "card_left": {
            "title": "1. ĐỊNH DẠNG BẢN TIN TRỰC QUAN & THẨM MỸ CAO",
            "bullets": [
                ("● Phân biệt khẩn cấp bằng Icon trực quan: ", "🚨🚨 Động Đất Nguy Hiểm (>0.5g) | 🔥🚨 Báo Cháy Khẩn Cấp (Lửa ADC < 2800) | ⚠️ Cảnh Báo Sớm | 🛠 Lỗi Cảm Biến."),
                ("● Dữ liệu đo đạc chi tiết từng thông số: ", "Thể hiện rõ tên trạm đo kitchen-01, gia tốc rung (g), chỉ số quang trở lửa, nhiệt độ môi trường (°C)."),
                ("● Tem thời gian chuẩn xác đến từng giây: ", "Cung cấp mốc thời gian thực phát hiện sự cố để lực lượng cứu hộ dễ dàng đối chiếu camera an ninh."),
                ("● Khuyến cáo sơ tán khẩn cấp: ", "Đính kèm chỉ dẫn an toàn ngay cuối tin nhắn ('Tìm nơi trú ẩn ngay!' hoặc 'Kiểm tra hiện trường chữa cháy!').")
            ]
        },
        "card_right": {
            "title": "2. ĐÁNH GIÁ HIỆU NĂNG THỰC NGHIỆM ĐO ĐẠC",
            "bullets": [
                ("● Độ trễ kích hoạt còi tại chỗ: ", "≈ 0ms (Còi và đèn hú tức thì ngay khi cảm biến chạm ngưỡng nguy hiểm, không bị trễ mạng)."),
                ("● Thời gian nhận tin trên điện thoại: ", "≈ 0.8s - 1.2s (Đã tính thời gian đóng gói JSON, gửi lên VPS, xếp hàng và đẩy qua Telegram)."),
                ("● Tỷ lệ chuyển phát thành công: ", "Đạt 100% trong 50 lượt thử nghiệm thực tế (bao gồm cả thử nghiệm ngắt kết nối mạng tạm thời)."),
                ("● Khả năng nhân rộng quy mô (Scalability): ", "Một máy chủ VPS có thể quản lý hàng trăm nút cảm biến ESP32 tại nhiều khu vực khác nhau.")
            ]
        }
    }
]

blank_layout = prs.slide_layouts[6] # Layout Blank

# Tạo 5 slide mới và căn chỉnh theo kích thước 20.0 x 11.25 inches
new_slide_elements = []
for d in slides_data:
    slide = prs.slides.add_slide(blank_layout)

    # 1. Header Box (Category & Title)
    header_box = slide.shapes.add_textbox(Inches(1.0), Inches(0.5), Inches(18.0), Inches(1.3))
    tf_h = header_box.text_frame
    tf_h.word_wrap = True

    # Category Line
    p_cat = tf_h.paragraphs[0]
    p_cat.text = d["category"].upper()
    p_cat.font.name = "Montserrat"
    p_cat.font.size = Pt(16)
    p_cat.font.bold = True
    p_cat.font.color.rgb = COLOR_NAVY_SUB
    p_cat.space_after = Pt(4)

    # Title Line
    p_title = tf_h.add_paragraph()
    p_title.text = d["title"]
    p_title.font.name = "Montserrat"
    p_title.font.size = Pt(28)
    p_title.font.bold = True
    p_title.font.color.rgb = COLOR_NAVY_TITLE

    # 2. Divider Line
    line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.0), Inches(1.95), Inches(18.0), Inches(0.04))
    line.fill.solid()
    line.fill.fore_color.rgb = COLOR_BORDER
    line.line.fill.background()

    # 3. Left Card Container
    card_l = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(1.0), Inches(2.2), Inches(8.8), Inches(8.4))
    card_l.fill.solid()
    card_l.fill.fore_color.rgb = COLOR_CARD_BG
    card_l.line.color.rgb = COLOR_BORDER
    card_l.line.width = Pt(1.5)

    tb_l = slide.shapes.add_textbox(Inches(1.3), Inches(2.45), Inches(8.2), Inches(7.9))
    tf_l = tb_l.text_frame
    tf_l.word_wrap = True

    p_lh = tf_l.paragraphs[0]
    p_lh.text = d["card_left"]["title"]
    p_lh.font.name = "Montserrat"
    p_lh.font.size = Pt(21)
    p_lh.font.bold = True
    p_lh.font.color.rgb = COLOR_NAVY_TITLE
    p_lh.space_after = Pt(14)

    for prefix, body in d["card_left"]["bullets"]:
        p = tf_l.add_paragraph()
        p.space_after = Pt(12)
        r_p = p.add_run()
        r_p.text = prefix
        r_p.font.name = "Montserrat"
        r_p.font.size = Pt(17.5)
        r_p.font.bold = True
        r_p.font.color.rgb = COLOR_NAVY_TITLE

        r_b = p.add_run()
        r_b.text = body
        r_b.font.name = "Montserrat"
        r_b.font.size = Pt(17.5)
        r_b.font.bold = False
        r_b.font.color.rgb = COLOR_TEXT_MAIN

    # 4. Right Card Container
    card_r = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(10.2), Inches(2.2), Inches(8.8), Inches(8.4))
    card_r.fill.solid()
    card_r.fill.fore_color.rgb = COLOR_CARD_BG
    card_r.line.color.rgb = COLOR_BORDER
    card_r.line.width = Pt(1.5)

    tb_r = slide.shapes.add_textbox(Inches(10.5), Inches(2.45), Inches(8.2), Inches(7.9))
    tf_r = tb_r.text_frame
    tf_r.word_wrap = True

    p_rh = tf_r.paragraphs[0]
    p_rh.text = d["card_right"]["title"]
    p_rh.font.name = "Montserrat"
    p_rh.font.size = Pt(21)
    p_rh.font.bold = True
    p_rh.font.color.rgb = COLOR_NAVY_TITLE
    p_rh.space_after = Pt(14)

    for prefix, body in d["card_right"]["bullets"]:
        p = tf_r.add_paragraph()
        p.space_after = Pt(12)
        r_p = p.add_run()
        r_p.text = prefix
        r_p.font.name = "Montserrat"
        r_p.font.size = Pt(17.5)
        r_p.font.bold = True
        r_p.font.color.rgb = COLOR_NAVY_TITLE

        r_b = p.add_run()
        r_b.text = body
        r_b.font.name = "Montserrat"
        r_b.font.size = Pt(17.5)
        r_b.font.bold = False
        r_b.font.color.rgb = COLOR_TEXT_MAIN

    new_slide_elements.append(prs.slides._sldIdLst[-1])

# Chèn 5 slide mới vào đúng vị trí ngay sau Slide 37 (index 37, 38, 39, 40, 41)
sldIdLst = prs.slides._sldIdLst
for idx_offset, elem in enumerate(new_slide_elements):
    target_pos = 37 + idx_offset
    sldIdLst.insert(target_pos, elem)

# LƯU Ý QUAN TRỌNG: KHÔNG CHẠM VÀO BẤT KỲ SLIDE CŨ NÀO (KHÔNG ĐỤNG FOOTER, TEXT, SHAPES CỦA 42 SLIDE GỐC)
prs.save(OUT_DESKTOP)
prs.save(OUT_DOWNLOADS)
prs.save(OUT_DOCS)

# Cập nhật đè lại file gốc C:\Users\Admin\Downloads\Nhom2.pptx
prs.save(SRC_PATH)

print(f"XONG! Tong so slide sau khi chen: {len(prs.slides)} (Goc 42 + 5 slide moi = 47 slides)")
print(f"File goc da duoc chen them 5 slide sau Slide 37: {SRC_PATH}")
print(f"File ban sao da tao tai Desktop: {OUT_DESKTOP}")
