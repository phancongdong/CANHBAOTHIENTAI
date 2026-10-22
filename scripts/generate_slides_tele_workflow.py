import pptx
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.enum.shapes import MSO_SHAPE
import os, re

# ==============================================================================
# 1. CẤU HÌNH MÀU SẮC VÀ THÔNG SỐ CHUẨN TỪ BÁO CÁO NHÓM 2
# ==============================================================================
COLOR_WHITE        = RGBColor(255, 255, 255)
COLOR_CARD_BG      = RGBColor(248, 250, 252) # #F8FAFC
COLOR_BORDER       = RGBColor(220, 226, 235) # #DCE2EB
COLOR_NAVY_HEADER  = RGBColor(0, 86, 145)    # #005691
COLOR_NAVY_TITLE   = RGBColor(27, 54, 93)    # #1B365D
COLOR_TEXT_MAIN    = RGBColor(34, 34, 34)    # #222222
COLOR_TEXT_MUTED   = RGBColor(102, 102, 102) # #666666

SOURCE_PPTX = r"C:\Users\Admin\Downloads\Bao_Cao_Nhom_2_38_Slide_Font_Lon.pptx"
if not os.path.exists(SOURCE_PPTX):
    SOURCE_PPTX = r"C:\Users\Admin\Desktop\Bao_Cao_Nhom_2_Hoan_Chinh.pptx"

OUT_DESKTOP_1 = r"C:\Users\Admin\Desktop\Bao_Cao_Nhom_2_43_Slide_Font_Lon.pptx"
OUT_DESKTOP_2 = r"C:\Users\Admin\Desktop\Bao_Cao_Nhom_2_Co_Workflow_Telegram.pptx"
OUT_DOCS = r"C:\CANHBAOTHIENTAI\docs\Bao_Cao_Nhom_2_43_Slide_Font_Lon.pptx"

prs = pptx.Presentation(SOURCE_PPTX)
blank_layout = prs.slide_layouts[6] # Blank layout

# Trích xuất logo từ slide 37
logo_blob = prs.slides[36].shapes[1].image.blob
temp_logo_path = "temp_gdu_logo.png"
with open(temp_logo_path, "wb") as f:
    f.write(logo_blob)

# ==============================================================================
# 2. DỮ LIỆU NỘI DUNG 5 SLIDE WORKFLOW TELEGRAM
# ==============================================================================
slides_data = [
    {
        "category": "PHẦN B: MỞ RỘNG IOT - HỆ THỐNG CẢNH BÁO THIÊN TAI VÀ HỎA HOẠN",
        "title": "TỔNG QUAN KIẾN TRÚC WORKFLOW GỬI CẢNH BÁO TELEGRAM",
        "card_left": {
            "title": "1. KIẾN TRÚC 4 TẦNG & LUỒNG TÍN HIỆU ĐẦU CUỐI (E2E)",
            "bullets": [
                ("● Tầng Thiết bị Biên (ESP32): ", "Quét cảm biến MPU-6050 (rung chấn), Flame AO (quang trở lửa), DHT11 (nhiệt độ). Bật ngay còi/LED 10Hz tại chỗ khi chạm ngưỡng."),
                ("● Tầng Truyền dẫn Mạng: ", "Đóng gói JSON kèm Bearer Token bảo mật, gửi qua Wi-Fi 2.4GHz với giao thức HTTP POST đến cổng 80 của VPS 103.178.235.184."),
                ("● Tầng Máy chủ Đám mây (VPS): ", "FastAPI tiếp nhận, xác thực bảo mật, lưu DB SQLite telemetry, lọc chống bão tin (Cooldown) và đẩy vào bảng alert_outbox."),
                ("● Tầng Thông báo Người dùng: ", "Dịch vụ Worker quét hàng đợi, gọi Telegram Bot API gửi Push Notification trực tiếp đến smartphone của người dùng.")
            ]
        },
        "card_right": {
            "title": "2. GIÁ TRỊ KỸ THUẬT & ƯU THẾ VƯỢT TRỘI CỦA HỆ THỐNG",
            "bullets": [
                ("● Khắc phục giới hạn còi tại chỗ: ", "Người dùng nhận cảnh báo tức thời ngay cả khi vắng nhà, đi công tác xa hoặc đang ngủ say."),
                ("● Độ trễ toàn trình cực thấp (0.8s - 1.5s): ", "Từ lúc cảm biến rung lắc/bắt lửa đến khi chuông điện thoại reo chỉ mất khoảng 1 giây."),
                ("● Tính độc lập & An toàn sinh mạng: ", "Nếu mất mạng Internet, còi và đèn tại chỗ vẫn hú 10Hz bình thường, bảo vệ an toàn 100%."),
                ("● Đa nền tảng, không cần cài App phụ: ", "Nhận thông báo trực tiếp qua ứng dụng Telegram trên iOS, Android, Windows, macOS và Smartwatch.")
            ]
        }
    },
    {
        "category": "PHẦN B: MỞ RỘNG IOT - TẦNG PHẦN CỨNG & XỬ LÝ BIÊN (EDGE LAYER)",
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
            "title": "2. PHÁT HIỆN CẢM BIẾN OFFLINE & ĐÓNG GÓI DỮ LIỆU JSON",
            "bullets": [
                ("● Giám sát bus I2C thời gian thực: ", "Hàm isAccelOnline() kiểm tra đường truyền SDA (21) / SCL (22) liên tục mỗi chu kỳ."),
                ("● Báo động sự cố tuột dây tức thì: ", "Nếu MPU-6050 bị lỏng dây hoặc Offline -> Còi hú 10Hz khẩn cấp tại chỗ, LCD in 'LOI PHAN CUNG! CAM BIEN OFFLINE'."),
                ("● Gói tin JSON chuẩn hóa công nghiệp: ", "Đóng gói device_id, chuỗi thứ tự seq, gia tốc vibration_g, chỉ số lửa flame_raw, nhiệt độ temperature_c."),
                ("● Xác thực Bearer Token: ", "Gắn mã định danh bí mật trong HTTP Header để máy chủ VPS phân quyền và từ chối các thiết bị lạ.")
            ]
        }
    },
    {
        "category": "PHẦN B: MỞ RỘNG IOT - TẦNG DỊCH VỤ ĐÁM MÂY (BACKEND CLOUD)",
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
        "category": "PHẦN B: MỞ RỘNG IOT - TẦNG BẢO ĐẢM TRUYỀN THÔNG TIN CẬY",
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
        "category": "PHẦN B: MỞ RỘNG IOT - GIAO DIỆN NGƯỜI DÙNG & ĐÁNH GIÁ THỰC NGHIỆM",
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

# ==============================================================================
# 3. HÀM TẠO SLIDE CHUẨN ĐỒNG BỘ GIAO DIỆN
# ==============================================================================
def create_styled_slide(slide, data, slide_num, total_slides):
    # 1. Background White
    bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(13.333), Inches(7.5))
    bg.fill.solid()
    bg.fill.fore_color.rgb = COLOR_WHITE
    bg.line.fill.background()

    # 2. Logo Top Right
    slide.shapes.add_picture(temp_logo_path, Inches(10.5), Inches(0.35), Inches(2.03), Inches(0.69))

    # 3. Header Category
    cat_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.24), Inches(9.5), Inches(0.32))
    tf_cat = cat_box.text_frame
    tf_cat.word_wrap = True
    p_cat = tf_cat.paragraphs[0]
    p_cat.text = data["category"].upper()
    p_cat.font.name = "Arial"
    p_cat.font.size = Pt(12)
    p_cat.font.bold = True
    p_cat.font.color.rgb = COLOR_NAVY_HEADER

    # 4. Main Slide Title
    title_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.55), Inches(9.5), Inches(0.75))
    tf_t = title_box.text_frame
    tf_t.word_wrap = True
    p_t = tf_t.paragraphs[0]
    p_t.text = data["title"]
    p_t.font.name = "Arial"
    p_t.font.size = Pt(22)
    p_t.font.bold = True
    p_t.font.color.rgb = COLOR_NAVY_TITLE

    # 5. Divider Line
    line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(1.36), Inches(11.73), Inches(0.02))
    line.fill.solid()
    line.fill.fore_color.rgb = COLOR_BORDER
    line.line.fill.background()

    # 6. Footer
    ft_box = slide.shapes.add_textbox(Inches(0.8), Inches(7.08), Inches(11.73), Inches(0.3))
    tf_ft = ft_box.text_frame
    p_ft = tf_ft.paragraphs[0]
    p_ft.text = f"Trường ĐH Gia Định | Cảm biến & Cơ cấu chấp hành (15113087) | Báo cáo Nhóm 2 - Chương 2 | Slide {slide_num}/{total_slides}"
    p_ft.font.name = "Arial"
    p_ft.font.size = Pt(11)
    p_ft.font.color.rgb = COLOR_TEXT_MUTED

    # 7 & 8: Left Card
    card_l = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.8), Inches(1.55), Inches(5.7), Inches(5.35))
    card_l.fill.solid()
    card_l.fill.fore_color.rgb = COLOR_CARD_BG
    card_l.line.color.rgb = COLOR_BORDER
    card_l.line.width = Pt(1.5)

    tb_l = slide.shapes.add_textbox(Inches(1.0), Inches(1.75), Inches(5.3), Inches(4.9))
    tf_l = tb_l.text_frame
    tf_l.word_wrap = True
    
    # Left Header
    p_lh = tf_l.paragraphs[0]
    p_lh.text = data["card_left"]["title"]
    p_lh.font.name = "Arial"
    p_lh.font.size = Pt(16)
    p_lh.font.bold = True
    p_lh.font.color.rgb = COLOR_NAVY_TITLE
    p_lh.space_after = Pt(10)

    for prefix, body in data["card_left"]["bullets"]:
        p = tf_l.add_paragraph()
        p.space_after = Pt(8)
        run_p = p.add_run()
        run_p.text = prefix
        run_p.font.name = "Arial"
        run_p.font.size = Pt(14)
        run_p.font.bold = True
        run_p.font.color.rgb = COLOR_NAVY_TITLE

        run_b = p.add_run()
        run_b.text = body
        run_b.font.name = "Arial"
        run_b.font.size = Pt(14)
        run_b.font.bold = False
        run_b.font.color.rgb = COLOR_TEXT_MAIN

    # 9 & 10: Right Card
    card_r = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(6.83), Inches(1.55), Inches(5.7), Inches(5.35))
    card_r.fill.solid()
    card_r.fill.fore_color.rgb = COLOR_CARD_BG
    card_r.line.color.rgb = COLOR_BORDER
    card_r.line.width = Pt(1.5)

    tb_r = slide.shapes.add_textbox(Inches(7.03), Inches(1.75), Inches(5.3), Inches(4.9))
    tf_r = tb_r.text_frame
    tf_r.word_wrap = True

    # Right Header
    p_rh = tf_r.paragraphs[0]
    p_rh.text = data["card_right"]["title"]
    p_rh.font.name = "Arial"
    p_rh.font.size = Pt(16)
    p_rh.font.bold = True
    p_rh.font.color.rgb = COLOR_NAVY_TITLE
    p_rh.space_after = Pt(10)

    for prefix, body in data["card_right"]["bullets"]:
        p = tf_r.add_paragraph()
        p.space_after = Pt(8)
        run_p = p.add_run()
        run_p.text = prefix
        run_p.font.name = "Arial"
        run_p.font.size = Pt(14)
        run_p.font.bold = True
        run_p.font.color.rgb = COLOR_NAVY_TITLE

        run_b = p.add_run()
        run_b.text = body
        run_b.font.name = "Arial"
        run_b.font.size = Pt(14)
        run_b.font.bold = False
        run_b.font.color.rgb = COLOR_TEXT_MAIN

# ==============================================================================
# 4. CHÈN 5 SLIDE VÀO SAU SLIDE 37 VÀ TÁI SẮP XẾP THỨ TỰ
# ==============================================================================
new_slides_elements = []
for i, d in enumerate(slides_data):
    s = prs.slides.add_slide(blank_layout)
    create_styled_slide(s, d, 38 + i, 43)
    new_slides_elements.append(prs.slides._sldIdLst[-1])

# Chuyển 5 slide mới vào đúng vị trí sau slide 37 (index 37..41)
sldIdLst = prs.slides._sldIdLst
for idx_offset, elem in enumerate(new_slides_elements):
    target_pos = 37 + idx_offset
    sldIdLst.insert(target_pos, elem)

# ==============================================================================
# 5. CẬP NHẬT LẠI SỐ TRANG TRÊN TOÀN BỘ CÁC SLIDE (SLIDE X/43)
# ==============================================================================
total_count = len(prs.slides) # 43
for idx, s in enumerate(prs.slides):
    slide_no = idx + 1
    for sh in s.shapes:
        if sh.has_text_frame and "Slide " in sh.text_frame.text:
            # Thay thế Slide X/38 hoặc Slide X/40 thành Slide X/43
            new_text = re.sub(r'Slide\s+\d+/\d+', f'Slide {slide_no}/{total_count}', sh.text_frame.text)
            sh.text_frame.text = new_text
            # Format lại font
            p = sh.text_frame.paragraphs[0]
            p.font.name = "Arial"
            p.font.size = Pt(11)
            p.font.color.rgb = COLOR_TEXT_MUTED

# ==============================================================================
# 6. LƯU TẬP TIN PPTX
# ==============================================================================
prs.save(OUT_DESKTOP_1)
prs.save(OUT_DESKTOP_2)
prs.save(OUT_DOCS)

if os.path.exists(temp_logo_path):
    os.remove(temp_logo_path)

print(f"Hoan tat! Tong so slide moi: {total_count}")
print(f"Da xuat ra:")
print(f"1. {OUT_DESKTOP_1}")
print(f"2. {OUT_DESKTOP_2}")
print(f"3. {OUT_DOCS}")
