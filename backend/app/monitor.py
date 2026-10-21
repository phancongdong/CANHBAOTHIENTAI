from datetime import datetime, timezone, timedelta
from typing import Optional
import sqlite3

from .db import get_connection, transaction
from .models import TelemetryEventIn
from .notifications import format_telegram_message

def monitor_liveness(
    conn: sqlite3.Connection,
    now: datetime,
    timeout_seconds: int = 60
) -> int:
    cutoff = (now - timedelta(seconds=timeout_seconds)).isoformat()
    now_iso = now.isoformat()

    marked_offline = 0
    with transaction(conn):
        cursor = conn.cursor()
        cursor.execute(
            """
            SELECT device_id, last_boot_id, last_seq, last_seen_at
            FROM device_state
            WHERE is_offline = 0
              AND last_seen_at < ?
            """,
            (cutoff,)
        )
        devices = cursor.fetchall()
        for dev in devices:
            device_id = dev["device_id"]
            boot_id = dev["last_boot_id"]
            seq = dev["last_seq"]

            # Mark offline
            cursor.execute(
                "UPDATE device_state SET is_offline = 1 WHERE device_id = ?",
                (device_id,)
            )

            # Insert offline notification
            notif_key = f"{device_id}:offline:{boot_id}:{seq}"
            dummy_ev = TelemetryEventIn(
                device_id=device_id,
                boot_id=boot_id,
                seq=seq,
                uptime_ms=0,
                sample_age_ms=0,
                sensor_fault=False,
                alarm_state="NORMAL",
                alarm_latched=False,
                event="heartbeat"
            )
            msg = format_telegram_message(dummy_ev, now, is_offline=True)

            cursor.execute(
                """
                INSERT OR IGNORE INTO outbox (
                    notification_key, device_id, event_type, incident_id,
                    message_text, payload, status, attempts, next_attempt_at, created_at
                ) VALUES (?, ?, 'offline', NULL, ?, '', 'pending', 0, ?, ?)
                """,
                (notif_key, device_id, msg, now_iso, now_iso)
            )
            marked_offline += 1

    return marked_offline

def monitor_reminders(
    conn: sqlite3.Connection,
    now: datetime,
    reminder_interval_seconds: int = 300
) -> int:
    cutoff = (now - timedelta(seconds=reminder_interval_seconds)).isoformat()
    now_iso = now.isoformat()

    reminders_created = 0
    with transaction(conn):
        cursor = conn.cursor()
        cursor.execute(
            """
            SELECT device_id, last_boot_id, last_seq, incident_id, last_temperature,
                   current_state, latched, last_alarm_reminded_at
            FROM device_state
            WHERE current_state = 'ALARM'
              AND (last_alarm_reminded_at IS NULL OR last_alarm_reminded_at < ?)
            """,
            (cutoff,)
        )
        alarmed_devices = cursor.fetchall()
        for dev in alarmed_devices:
            device_id = dev["device_id"]
            inc_id = dev["incident_id"] or "incident"

            # Create periodic reminder notification key (max 1 per interval window)
            reminder_slot = int(now.timestamp() // reminder_interval_seconds)
            notif_key = f"{device_id}:{inc_id}:reminder:{reminder_slot}"

            ev = TelemetryEventIn(
                device_id=device_id,
                boot_id=dev["last_boot_id"],
                seq=dev["last_seq"],
                uptime_ms=0,
                sample_age_ms=0,
                temperature_c=dev["last_temperature"],
                flame_detected=True,
                sensor_fault=False,
                alarm_state="ALARM",
                alarm_latched=True,
                event="alarm_started",
                incident_id=dev["incident_id"]
            )
            msg = format_telegram_message(ev, now, reminder=True)

            cursor.execute(
                """
                INSERT OR IGNORE INTO outbox (
                    notification_key, device_id, event_type, incident_id,
                    message_text, payload, status, attempts, next_attempt_at, created_at
                ) VALUES (?, ?, 'alarm_reminder', ?, ?, '', 'pending', 0, ?, ?)
                """,
                (notif_key, device_id, inc_id, msg, now_iso, now_iso)
            )

            cursor.execute(
                "UPDATE device_state SET last_alarm_reminded_at = ? WHERE device_id = ?",
                (now_iso, device_id)
            )
            reminders_created += 1

    return reminders_created
