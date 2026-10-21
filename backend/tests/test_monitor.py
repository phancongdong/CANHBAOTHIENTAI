from datetime import datetime, timezone, timedelta
import pytest
from app.db import get_connection
from app.monitor import monitor_liveness, monitor_reminders
from app.ingest import ingest_event
from app.models import TelemetryEventIn

def init_device_state(db_path, device_id="kitchen-01", last_seen_ago_sec=70, state="NORMAL"):
    t = datetime.now(timezone.utc) - timedelta(seconds=last_seen_ago_sec)
    conn = get_connection(db_path)
    cursor = conn.cursor()
    cursor.execute(
        """
        INSERT INTO device_state (
            device_id, last_boot_id, last_seq, last_seen_at, current_state,
            latched, incident_id, last_temperature, sensor_fault, is_offline
        ) VALUES (?, 'boot-1', 10, ?, ?, 0, 'inc-1', 25.0, 0, 0)
        """,
        (device_id, t.isoformat(), state)
    )
    conn.commit()
    conn.close()

def test_offline_detection_and_no_duplicate(temp_db):
    init_device_state(temp_db, device_id="kitchen-01", last_seen_ago_sec=75)
    now = datetime.now(timezone.utc)

    conn = get_connection(temp_db)
    # First check: marks device offline and generates 1 notification
    marked = monitor_liveness(conn, now, timeout_seconds=60)
    assert marked == 1

    cursor = conn.cursor()
    cursor.execute("SELECT is_offline FROM device_state WHERE device_id = 'kitchen-01'")
    assert cursor.fetchone()["is_offline"] == 1

    cursor.execute("SELECT COUNT(*) FROM outbox WHERE event_type = 'offline'")
    assert cursor.fetchone()[0] == 1

    # Second check 10 seconds later: should NOT create duplicate offline notification
    marked_second = monitor_liveness(conn, now + timedelta(seconds=10), timeout_seconds=60)
    assert marked_second == 0
    cursor.execute("SELECT COUNT(*) FROM outbox WHERE event_type = 'offline'")
    assert cursor.fetchone()[0] == 1
    conn.close()

def test_recovery_notification_after_offline(temp_db):
    init_device_state(temp_db, device_id="kitchen-01", last_seen_ago_sec=80)
    now = datetime.now(timezone.utc)

    conn = get_connection(temp_db)
    monitor_liveness(conn, now, timeout_seconds=60)
    cursor = conn.cursor()
    cursor.execute("SELECT is_offline FROM device_state WHERE device_id = 'kitchen-01'")
    assert cursor.fetchone()["is_offline"] == 1

    # Fresh heartbeat comes in
    fresh_now = now + timedelta(seconds=10)
    ev = TelemetryEventIn(
        device_id="kitchen-01",
        boot_id="boot-1",
        seq=11,
        uptime_ms=20000,
        sample_age_ms=0,
        temperature_c=25.0,
        flame_detected=False,
        sensor_fault=False,
        alarm_state="NORMAL",
        alarm_latched=False,
        event="heartbeat"
    )
    ingest_event(conn, ev, ev.model_dump_json(), fresh_now)

    cursor.execute("SELECT is_offline FROM device_state WHERE device_id = 'kitchen-01'")
    assert cursor.fetchone()["is_offline"] == 0

    cursor.execute("SELECT COUNT(*) FROM outbox WHERE event_type = 'recovery'")
    assert cursor.fetchone()[0] == 1
    conn.close()

def test_alarm_reminder_periodic(temp_db):
    init_device_state(temp_db, device_id="kitchen-01", last_seen_ago_sec=10, state="ALARM")
    conn = get_connection(temp_db)

    t0 = datetime.now(timezone.utc)
    # First reminder triggers
    reminders = monitor_reminders(conn, t0, reminder_interval_seconds=300)
    assert reminders == 1

    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM outbox WHERE event_type = 'alarm_reminder'")
    assert cursor.fetchone()[0] == 1

    # Before 300 seconds (e.g. 100 seconds later), no new reminder
    reminders_too_soon = monitor_reminders(conn, t0 + timedelta(seconds=100), reminder_interval_seconds=300)
    assert reminders_too_soon == 0

    # After 301 seconds, new reminder triggers
    reminders_due = monitor_reminders(conn, t0 + timedelta(seconds=305), reminder_interval_seconds=300)
    assert reminders_due == 1

    cursor.execute("SELECT COUNT(*) FROM outbox WHERE event_type = 'alarm_reminder'")
    assert cursor.fetchone()[0] == 2
    conn.close()
