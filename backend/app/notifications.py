from datetime import datetime, timezone
from typing import Optional
from .models import TelemetryEventIn

def format_telegram_message(
    event: TelemetryEventIn,
    received_at: datetime,
    reminder: bool = False,
    is_offline: Optional[bool] = None
) -> str:
    timestamp_str = received_at.strftime("%Y-%m-%d %H:%M:%S UTC")

    if is_offline is True:
        return (
            f"⚠️ <b>[CẢNH BÁO MẤT KẾT NỐI]</b>\n"
            f"📍 <b>Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"⏱ <b>Thời điểm ghi nhận:</b> {timestamp_str}\n"
            f"ℹ️ Thiết bị không gửi tín hiệu trong hơn 60 giây. Hãy kiểm tra nguồn điện và Wi-Fi."
        )

    if is_offline is False:
        return (
            f"✅ <b>[KẾT NỐI ĐÃ PHỤC HỒI]</b>\n"
            f"📍 <b>Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"⏱ <b>Thời điểm phục hồi:</b> {timestamp_str}\n"
            f"ℹ️ Thiết bị đã kết nối lại và gửi tín hiệu bình thường."
        )

    vib_str = f"{event.vibration_g:.2f}g" if event.vibration_g is not None else "N/A"
    flame_str = f"{event.flame_raw}" if event.flame_raw is not None else "N/A"
    temp_str = f"{event.temperature_c:.1f} °C" if event.temperature_c is not None else "N/A"

    inc_info = f"🆔 <b>Mã sự cố:</b> <code>{event.incident_id}</code>\n" if event.incident_id else ""

    details = (
        f"{inc_info}"
        f"📊 <b>Thông số cảm biến:</b>\n"
        f"  • Rung động (RĐ): <code>{vib_str}</code>\n"
        f"  • Ngọn lửa (NL): <code>{flame_str}</code>\n"
        f"  • Nhiệt độ (NĐ): <code>{temp_str}</code>\n"
        f"📺 <b>Hiển thị LCD:</b> <code>{event.lcd_message or 'N/A'}</code>\n"
        f"⏱ <b>Thời gian máy chủ:</b> {timestamp_str}"
    )

    ev_name = event.event
    if ev_name == "earthquake":
        return (
            f"🚨🚨 <b>[CẢNH BÁO ĐỘNG ĐẤT NGUY HIỂM]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📢 <b>Còi & LED:</b> BẬT 10Hz (Khẩn cấp)\n"
            f"⚠️ <b>Phát hiện:</b> Rung động > 0.5g ({vib_str})!\n"
            f"{details}\n"
            f"⚠️ <i>Hãy tìm nơi trú ẩn an toàn ngay lập tức!</i>"
        )
    elif ev_name in ("fire_alarm", "alarm_started"):
        prefix = "🚨 <b>[NHẮC NHỞ] ĐÁM CHÁY CHƯA DẬP TẮT</b>" if reminder else "🔥🚨 <b>[CẢNH BÁO CHÁY KHẨN CẤP]</b>"
        return (
            f"{prefix}\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📢 <b>Còi & LED:</b> BẬT 10Hz (Khẩn cấp)\n"
            f"🔥 <b>Phát hiện:</b> Cảm biến lửa < 1600 ({flame_str})!\n"
            f"{details}\n"
            f"⚠️ <i>Hãy sơ tán và xử lý chữa cháy khẩn cấp!</i>"
        )
    elif ev_name == "temp_high":
        return (
            f"🌡🚨 <b>[CẢNH BÁO NHIỆT ĐỘ QUÁ CAO]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📢 <b>Còi & LED:</b> BẬT 10Hz (Khẩn cấp)\n"
            f"🌡 <b>Phát hiện:</b> Nhiệt độ > 45°C ({temp_str})!\n"
            f"{details}"
        )
    elif ev_name == "vibration_light":
        return (
            f"⚠️ <b>[CẢNH BÁO RUNG NHẸ]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📢 <b>Còi & LED:</b> BẬT 1Hz (Cảnh báo)\n"
            f"ℹ️ <b>Phát hiện:</b> 0.2g <= RĐ <= 0.5g ({vib_str})\n"
            f"{details}"
        )
    elif ev_name == "flame_early":
        return (
            f"⚠️ <b>[CẢNH BÁO SỚM NGỌN LỬA]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📢 <b>Còi & LED:</b> BẬT 1Hz (Cảnh báo sớm)\n"
            f"ℹ️ <b>Phát hiện:</b> 1600 <= NL <= 3200 ({flame_str})\n"
            f"{details}"
        )
    elif ev_name == "temp_medium":
        return (
            f"⚠️ <b>[CẢNH BÁO NHIỆT ĐỘ TRUNG BÌNH]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📢 <b>Còi & LED:</b> BẬT 1Hz (Cảnh báo)\n"
            f"ℹ️ <b>Phát hiện:</b> 38°C <= NĐ <= 45°C ({temp_str})\n"
            f"{details}"
        )
    elif ev_name in ("safe", "alarm_cleared"):
        return (
            f"✅ <b>[BÁO ĐỘNG ĐÃ ĐƯỢC XÓA AN TOÀN - HỆ THỐNG AN TOÀN]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"🔇 <b>Còi & LED:</b> ĐÃ TẮT (OFF)\n"
            f"{details}\n"
            f"ℹ️ Toàn bộ các chỉ số đã trở lại mức bình thường an toàn."
        )
    elif ev_name == "sensor_fault":
        return (
            f"⚠️ <b>[CẢNH BÁO LỖI KẾT NỐI CẢM BIẾN]</b>\n"
            f"📍 <b>Khu vực:</b> <code>{event.device_id}</code>\n"
            f"📺 <b>Hiển thị LCD:</b> <code>Thông Báo Lỗi</code>\n"
            f"⏱ <b>Thời điểm ghi nhận:</b> {timestamp_str}\n"
            f"ℹ️ Mất kết nối cảm biến gia tốc, ngọn lửa hoặc nhiệt độ. Cần kiểm tra dây nối."
        )
    else:
        return f"ℹ️ Thiết bị <code>{event.device_id}</code> cập nhật trạng thái: {ev_name} ({timestamp_str})"
