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
            f"ℹ️ Thiết bị không gửi nhịp tim trong hơn 60 giây. Hãy kiểm tra nguồn điện và Wi-Fi."
        )

    if is_offline is False:
        return (
            f"✅ <b>[KẾT NỐI ĐÃ PHỤC HỒI]</b>\n"
            f"📍 <b>Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"⏱ <b>Thời điểm phục hồi:</b> {timestamp_str}\n"
            f"ℹ️ Thiết bị đã kết nối lại và gửi tín hiệu bình thường."
        )

    temp_str = f"{event.temperature_c:.1f} °C" if event.temperature_c is not None else "Không xác định (Lỗi)"
    flame_str = "CÓ PHÁT HIỆN LỬA 🔥" if event.flame_detected else "Không có ngọn lửa"

    if event.event == "alarm_started":
        prefix = "🚨 <b>[NHẮC NHỞ] ĐÁM CHÁY CHƯA DẬP TẮT</b>" if reminder else "🔥🚨 <b>[BÁO CHÁY KHẨN CẤP]</b>"
        return (
            f"{prefix}\n"
            f"📍 <b>Khu vực / Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"🆔 <b>Mã sự cố:</b> <code>{event.incident_id or 'N/A'}</code>\n"
            f"🌡 <b>Nhiệt độ:</b> {temp_str}\n"
            f"🔥 <b>Cảm biến lửa:</b> {flame_str}\n"
            f"📢 <b>Còi tại chỗ:</b> ĐANG HÚ BÁO ĐỘNG\n"
            f"⏱ <b>Thời gian máy chủ:</b> {timestamp_str}\n"
            f"⚠️ <i>Hãy sơ tán và xử lý chữa cháy ngay lập tức!</i>"
        )
    elif event.event == "alarm_cleared":
        return (
            f"✅ <b>[BÁO ĐỘNG ĐÃ ĐƯỢC XÓA AN TOÀN]</b>\n"
            f"📍 <b>Khu vực / Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"🆔 <b>Mã sự cố:</b> <code>{event.incident_id or 'N/A'}</code>\n"
            f"🌡 <b>Nhiệt độ hiện tại:</b> {temp_str}\n"
            f"🔥 <b>Cảm biến lửa:</b> {flame_str}\n"
            f"⏱ <b>Thời gian ghi nhận:</b> {timestamp_str}\n"
            f"ℹ️ Nhiệt độ đã về mức an toàn và đám cháy đã được dập tắt."
        )
    elif event.event == "sensor_fault":
        return (
            f"⚠️ <b>[CẢNH BÁO LỖI PHẦN CỨNG CẢM BIẾN]</b>\n"
            f"📍 <b>Khu vực / Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"⚠️ <b>Tình trạng:</b> Mất kết nối cảm biến nhiệt độ hoặc cảm biến lửa.\n"
            f"⏱ <b>Thời gian ghi nhận:</b> {timestamp_str}\n"
            f"ℹ️ Cần cử người kiểm tra dây dẫn hoặc linh kiện tại hiện trường."
        )
    elif event.event == "sensor_recovered":
        return (
            f"🟢 <b>[CẢM BIẾN ĐÃ PHỤC HỒI]</b>\n"
            f"📍 <b>Khu vực / Thiết bị:</b> <code>{event.device_id}</code>\n"
            f"🌡 <b>Nhiệt độ:</b> {temp_str}\n"
            f"⏱ <b>Thời gian ghi nhận:</b> {timestamp_str}\n"
            f"ℹ️ Toàn bộ cảm biến đã hoạt động ổn định trở lại."
        )
    else:
        return f"ℹ️ Thiết bị <code>{event.device_id}</code> gửi sự kiện: {event.event} ({timestamp_str})"
