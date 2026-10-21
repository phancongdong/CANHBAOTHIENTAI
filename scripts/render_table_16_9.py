import os
from PIL import Image, ImageDraw, ImageFont

def render_table():
    # 16:9 aspect ratio: 1920 x 1080
    WIDTH = 1920
    HEIGHT = 1080
    
    img = Image.new("RGB", (WIDTH, HEIGHT), color="#f1f5f9")
    draw = ImageDraw.Draw(img)

    # Fonts
    font_path_bold = "C:/Windows/Fonts/segoeuib.ttf"
    font_path_reg = "C:/Windows/Fonts/segoeui.ttf"
    
    if not os.path.exists(font_path_bold):
        font_path_bold = "C:/Windows/Fonts/arialbd.ttf"
        font_path_reg = "C:/Windows/Fonts/arial.ttf"

    title_font = ImageFont.truetype(font_path_bold, 36)
    sub_font = ImageFont.truetype(font_path_reg, 20)
    header_font = ImageFont.truetype(font_path_bold, 24)
    comp_font = ImageFont.truetype(font_path_bold, 21)
    cell_font = ImageFont.truetype(font_path_reg, 20)
    cell_bold_font = ImageFont.truetype(font_path_bold, 20)

    # Title Banner
    title_text = "SƠ ĐỒ ĐẤU NỐI CHÂN HỆ THỐNG CẢNH BÁO THIÊN TAI (ESP32)"
    sub_text = "Cấu hình chuẩn phần cứng ESP32 DevKit • Cảm biến MPU6050 • DHT11 • Lửa Analog • Còi & LED 1602 LCD"
    
    draw.text((WIDTH // 2, 45), title_text, fill="#1e3a8a", font=title_font, anchor="mm")
    draw.text((WIDTH // 2, 85), sub_text, fill="#475569", font=sub_font, anchor="mm")

    # Table Geometry
    table_x = 70
    table_y = 115
    table_w = WIDTH - 140  # 1780 px
    
    col_w1 = 440  # Linh kiện
    col_w2 = 460  # Chân trên linh kiện
    col_w3 = table_w - col_w1 - col_w2  # 880 px (Vị trí kết nối)

    # Colors
    HEADER_BG = "#2563eb"       # Vibrant Blue
    COMP_BG = "#3b82f6"         # Clean Medium Blue
    BORDER_COLOR = "#94a3b8"    # Slate border
    ROW_EVEN_BG = "#ffffff"     # White
    ROW_ODD_BG = "#f8fafc"      # Very subtle slate
    TEXT_DARK = "#0f172a"
    TEXT_WHITE = "#ffffff"
    PIN_HIGHLIGHT = "#b91c1c"   # Red accent for pin numbers

    # Component Groups
    components = [
        {
            "name": "Cấp nguồn chung\n(Breadboard)",
            "rows": [
                ("VIN (hoặc 5V từ ESP32)", "Hàng nguồn Đỏ (+) bên dưới Breadboard"),
                ("GND (Từ ESP32)", "Hàng nguồn Xanh (-) bên dưới Breadboard")
            ]
        },
        {
            "name": "Cảm biến gia tốc rung động\n(MPU-6050 / GY-521)",
            "rows": [
                ("VCC", "Hàng nguồn Đỏ (+) của Breadboard (3.3V hoặc 5V)"),
                ("GND", "Hàng nguồn Xanh (-) của Breadboard"),
                ("SCL", "Chân GPIO 22 (D22) của ESP32"),
                ("SDA", "Chân GPIO 21 (D21) của ESP32")
            ]
        },
        {
            "name": "Màn hình LCD 1602\n(Module I2C)",
            "rows": [
                ("VCC", "Hàng nguồn Đỏ (+) của Breadboard (Nguồn 5V/VIN sáng rõ)"),
                ("GND", "Hàng nguồn Xanh (-) của Breadboard"),
                ("SDA", "Chân GPIO 21 (D21) [Nối song song đường I2C]"),
                ("SCL", "Chân GPIO 22 (D22) [Nối song song đường I2C]")
            ]
        },
        {
            "name": "Cảm biến nhiệt độ\n(DHT11)",
            "rows": [
                ("VCC (+)", "Hàng nguồn Đỏ (+) của Breadboard"),
                ("GND (-)", "Hàng nguồn Xanh (-) của Breadboard"),
                ("DATA (hoặc OUT)", "Chân GPIO 4 (D4) của ESP32")
            ]
        },
        {
            "name": "Cảm biến ngọn lửa\n(Flame Sensor Module)",
            "rows": [
                ("VCC", "Hàng nguồn Đỏ (+) của Breadboard"),
                ("GND", "Hàng nguồn Xanh (-) của Breadboard"),
                ("A0 (Analog Out)", "Chân GPIO 34 (D34) của ESP32 (ADC1_CH6)")
            ]
        },
        {
            "name": "Còi báo động\n(Active Buzzer)",
            "rows": [
                ("Chân Dương (+) / Dây Đỏ", "Chân GPIO 25 (D25) của ESP32"),
                ("Chân Âm (-) / Dây Đen", "Hàng nguồn Xanh (-) của Breadboard")
            ]
        },
        {
            "name": "Đèn LED cảnh báo\n(Màu Đỏ)",
            "rows": [
                ("Chân Dương (+) [Chân dài]", "Chân GPIO 26 (D26) của ESP32"),
                ("Chân Âm (-) [Chân ngắn]", "Nối qua trở 220Ω vào Hàng nguồn Xanh (-) Breadboard")
            ]
        },
        {
            "name": "Nút nhấn Test / Reset\n(Tùy chọn)",
            "rows": [
                ("Chân 1", "Chân GPIO 15 (D15) của ESP32"),
                ("Chân 2", "Hàng nguồn Xanh (-) của Breadboard")
            ]
        }
    ]

    total_subrows = sum(len(c["rows"]) for c in components)  # 23 rows
    header_h = 50
    available_h = HEIGHT - table_y - 45  # 920 px
    row_h = (available_h - header_h) / total_subrows  # approx 37.8 px per row

    # Draw Header Row
    draw.rectangle([table_x, table_y, table_x + col_w1, table_y + header_h], fill=HEADER_BG, outline=BORDER_COLOR, width=2)
    draw.rectangle([table_x + col_w1, table_y, table_x + col_w1 + col_w2, table_y + header_h], fill=HEADER_BG, outline=BORDER_COLOR, width=2)
    draw.rectangle([table_x + col_w1 + col_w2, table_y, table_x + table_w, table_y + header_h], fill=HEADER_BG, outline=BORDER_COLOR, width=2)

    draw.text((table_x + col_w1 / 2, table_y + header_h / 2), "Linh kiện", fill=TEXT_WHITE, font=header_font, anchor="mm")
    draw.text((table_x + col_w1 + col_w2 / 2, table_y + header_h / 2), "Chân trên linh kiện", fill=TEXT_WHITE, font=header_font, anchor="mm")
    draw.text((table_x + col_w1 + col_w2 + col_w3 / 2, table_y + header_h / 2), "Vị trí kết nối (ESP32 / Breadboard)", fill=TEXT_WHITE, font=header_font, anchor="mm")

    # Draw Component Rows
    current_y = table_y + header_h
    row_idx = 0

    for comp in components:
        num_rows = len(comp["rows"])
        comp_h = num_rows * row_h
        comp_bottom = current_y + comp_h

        # Column 1 (Merged Cell for Component)
        draw.rectangle([table_x, current_y, table_x + col_w1, comp_bottom], fill=COMP_BG, outline=BORDER_COLOR, width=2)
        
        # Multiline text in Column 1
        comp_lines = comp["name"].split("\n")
        line_offset = - (len(comp_lines) - 1) * 14
        for l in comp_lines:
            draw.text((table_x + col_w1 / 2, current_y + comp_h / 2 + line_offset), l, fill=TEXT_WHITE, font=comp_font, anchor="mm")
            line_offset += 28

        # Sub-rows for Col 2 & Col 3
        for i, (pin_name, target_pos) in enumerate(comp["rows"]):
            sub_top = current_y + i * row_h
            sub_bottom = sub_top + row_h
            bg = ROW_EVEN_BG if row_idx % 2 == 0 else ROW_ODD_BG
            row_idx += 1

            # Col 2
            draw.rectangle([table_x + col_w1, sub_top, table_x + col_w1 + col_w2, sub_bottom], fill=bg, outline=BORDER_COLOR, width=1)
            draw.text((table_x + col_w1 + 35, sub_top + row_h / 2), pin_name, fill=TEXT_DARK, font=cell_bold_font, anchor="lm")

            # Col 3
            draw.rectangle([table_x + col_w1 + col_w2, sub_top, table_x + table_w, sub_bottom], fill=bg, outline=BORDER_COLOR, width=1)
            draw.text((table_x + col_w1 + col_w2 + 35, sub_top + row_h / 2), target_pos, fill=TEXT_DARK, font=cell_font, anchor="lm")

        current_y = comp_bottom

    # Outer border for whole table
    draw.rectangle([table_x, table_y, table_x + table_w, current_y], outline="#1e40af", width=3)

    output_path = "C:/CANHBAOTHIENTAI/SoDoNoiChan_ESP32_16x9.png"
    img.save(output_path, "PNG", quality=95)
    print(f"Saved image to: {output_path}")
    return output_path

if __name__ == "__main__":
    render_table()
