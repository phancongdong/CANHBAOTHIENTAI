import json
import hashlib
from datetime import datetime, timezone
import sqlite3
from fastapi import HTTPException
from .models import TelemetryEventIn, TelemetryResponse
from .db import transaction
from .notifications import format_telegram_message

def ingest_event(
    conn: sqlite3.Connection,
    event: TelemetryEventIn,
    raw_payload: str,
    now: datetime
) -> TelemetryResponse:
    payload_hash = hashlib.sha256(raw_payload.encode("utf-8")).hexdigest()
    now_iso = now.isoformat()

    # 1. Check idempotency
    cursor = conn.cursor()
    cursor.execute(
        "SELECT payload_hash FROM events WHERE device_id = ? AND boot_id = ? AND seq = ?",
        (event.device_id, event.boot_id, event.seq)
    )
    existing = cursor.fetchone()

    if existing:
        if existing["payload_hash"] == payload_hash:
            return TelemetryResponse(accepted=True, duplicate=True)
        else:
            raise HTTPException(
                status_code=409,
                detail="Conflict: Same sequence received with different payload hash"
            )

    # 2. Insert and state update within a single transaction
    with transaction(conn):
        cursor.execute(
            """
            INSERT INTO events (device_id, boot_id, seq, received_at, payload, payload_hash)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            (event.device_id, event.boot_id, event.seq, now_iso, raw_payload, payload_hash)
        )

        cursor.execute(
            "SELECT last_boot_id, last_seq, is_offline, current_state FROM device_state WHERE device_id = ?",
            (event.device_id,)
        )
        prev_state = cursor.fetchone()

        is_newer = True
        was_offline = False
        if prev_state:
            was_offline = bool(prev_state["is_offline"])
            if prev_state["last_boot_id"] == event.boot_id and event.seq < prev_state["last_seq"]:
                is_newer = False

        if is_newer:
            cursor.execute(
                """
                INSERT INTO device_state (
                    device_id, last_boot_id, last_seq, last_seen_at, current_state,
                    latched, incident_id, last_temperature, sensor_fault, is_offline
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0)
                ON CONFLICT(device_id) DO UPDATE SET
                    last_boot_id = excluded.last_boot_id,
                    last_seq = excluded.last_seq,
                    last_seen_at = excluded.last_seen_at,
                    current_state = excluded.current_state,
                    latched = excluded.latched,
                    incident_id = excluded.incident_id,
                    last_temperature = excluded.last_temperature,
                    sensor_fault = excluded.sensor_fault,
                    is_offline = 0
                """,
                (
                    event.device_id, event.boot_id, event.seq, now_iso, event.alarm_state,
                    1 if event.alarm_latched else 0, event.incident_id,
                    event.temperature_c, 1 if event.sensor_fault else 0
                )
            )

        # If device was marked offline and now sends fresh telemetry -> trigger recovery notification
        if was_offline:
            rec_key = f"{event.device_id}:recovery:{event.boot_id}:{event.seq}"
            rec_msg = format_telegram_message(event, now, is_offline=False)
            cursor.execute(
                """
                INSERT OR IGNORE INTO outbox (
                    notification_key, device_id, event_type, incident_id,
                    message_text, payload, status, attempts, next_attempt_at, created_at
                ) VALUES (?, ?, 'recovery', ?, ?, ?, 'pending', 0, ?, ?)
                """,
                (rec_key, event.device_id, event.incident_id, rec_msg, raw_payload, now_iso, now_iso)
            )

        # 3. Handle Notification Outbox
        if event.event in ["alarm_started", "alarm_cleared", "sensor_fault", "sensor_recovered"]:
            if event.event == "alarm_started":
                inc_tag = event.incident_id or f"boot-{event.boot_id}-seq-{event.seq}"
                notif_key = f"{event.device_id}:{inc_tag}:alarm_started"
            elif event.event == "alarm_cleared":
                inc_tag = event.incident_id or f"boot-{event.boot_id}-seq-{event.seq}"
                notif_key = f"{event.device_id}:{inc_tag}:alarm_cleared"
            else:
                notif_key = f"{event.device_id}:{event.event}:{event.boot_id}:{event.seq}"

            msg = format_telegram_message(event, now)
            cursor.execute(
                """
                INSERT OR IGNORE INTO outbox (
                    notification_key, device_id, event_type, incident_id,
                    message_text, payload, status, attempts, next_attempt_at, created_at
                ) VALUES (?, ?, ?, ?, ?, ?, 'pending', 0, ?, ?)
                """,
                (notif_key, event.device_id, event.event, event.incident_id, msg, raw_payload, now_iso, now_iso)
            )

    return TelemetryResponse(accepted=True, duplicate=False)
