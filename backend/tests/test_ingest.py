import sqlite3
import pytest
from app.db import get_connection

def sample_event(seq=1, event="heartbeat", temp=26.0, inc=None):
    return {
        "schema_version": 1,
        "device_id": "kitchen-01",
        "boot_id": "boot-000001",
        "seq": seq,
        "uptime_ms": seq * 1000,
        "sample_age_ms": 0,
        "temperature_c": temp,
        "flame_detected": False,
        "sensor_fault": False,
        "alarm_state": "NORMAL",
        "alarm_latched": False,
        "event": event,
        "incident_id": inc
    }

def test_heartbeat_ingest_no_outbox(client, auth_headers, temp_db):
    payload = sample_event(seq=1, event="heartbeat")
    res = client.post("/api/v1/telemetry", headers=auth_headers, json=payload)
    assert res.status_code == 200
    assert res.json() == {"accepted": True, "duplicate": False}

    conn = get_connection(temp_db)
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM events")
    assert cursor.fetchone()[0] == 1

    cursor.execute("SELECT COUNT(*) FROM outbox")
    assert cursor.fetchone()[0] == 0  # Heartbeat should not create outbox notification
    conn.close()

def test_alarm_started_creates_outbox_and_idempotent(client, auth_headers, temp_db):
    payload = sample_event(seq=2, event="alarm_started", temp=58.0, inc="inc-001")
    payload["alarm_state"] = "ALARM"
    payload["alarm_latched"] = True
    payload["flame_detected"] = True

    # First send: accepted, duplicate=False
    res1 = client.post("/api/v1/telemetry", headers=auth_headers, json=payload)
    assert res1.status_code == 200
    assert res1.json() == {"accepted": True, "duplicate": False}

    conn = get_connection(temp_db)
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM events")
    assert cursor.fetchone()[0] == 1
    cursor.execute("SELECT COUNT(*) FROM outbox")
    assert cursor.fetchone()[0] == 1

    # Second send (exact duplicate): accepted, duplicate=True, outbox remains 1
    res2 = client.post("/api/v1/telemetry", headers=auth_headers, json=payload)
    assert res2.status_code == 200
    assert res2.json() == {"accepted": True, "duplicate": True}

    cursor.execute("SELECT COUNT(*) FROM outbox")
    assert cursor.fetchone()[0] == 1
    conn.close()

def test_payload_conflict_on_same_sequence(client, auth_headers, temp_db):
    payload1 = sample_event(seq=5, event="heartbeat", temp=25.0)
    res1 = client.post("/api/v1/telemetry", headers=auth_headers, json=payload1)
    assert res1.status_code == 200

    # Same sequence 5, but different temperature (different hash)
    payload2 = sample_event(seq=5, event="heartbeat", temp=35.0)
    res2 = client.post("/api/v1/telemetry", headers=auth_headers, json=payload2)
    assert res2.status_code == 409
    assert "Conflict" in res2.json()["detail"]

def test_stale_event_does_not_rollback_state(client, auth_headers, temp_db):
    # Send seq 10
    ev10 = sample_event(seq=10, event="heartbeat", temp=26.0)
    client.post("/api/v1/telemetry", headers=auth_headers, json=ev10)

    # Send older seq 5
    ev5 = sample_event(seq=5, event="heartbeat", temp=20.0)
    client.post("/api/v1/telemetry", headers=auth_headers, json=ev5)

    conn = get_connection(temp_db)
    cursor = conn.cursor()
    cursor.execute("SELECT last_seq, last_temperature FROM device_state WHERE device_id = 'kitchen-01'")
    row = cursor.fetchone()
    assert row["last_seq"] == 10
    assert row["last_temperature"] == 26.0
    conn.close()
