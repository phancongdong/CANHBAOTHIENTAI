import tempfile
import os
import pytest
from datetime import datetime, timezone
from fastapi.testclient import TestClient

from app.main import app
from app.db import init_db, get_connection
import app.config as config
import app.main as main_module
from app.worker import run_once
from app.monitor import monitor_liveness

class MockTelegramClient:
    def __init__(self):
        self.sent_messages = []

    def post(self, url, json=None):
        self.sent_messages.append(json)
        class MockResp:
            status_code = 200
            def json(self):
                return {"ok": True}
        return MockResp()

def test_full_e2e_alarm_to_telegram_cycle(temp_db):
    client = TestClient(app)
    auth = {"Authorization": "Bearer secret-token-kitchen-01"}
    fake_tele = MockTelegramClient()

    # 1. Normal Heartbeat
    hb_payload = {
        "schema_version": 1,
        "device_id": "kitchen-01",
        "boot_id": "boot-e2e-01",
        "seq": 1,
        "uptime_ms": 1000,
        "sample_age_ms": 0,
        "temperature_c": 25.0,
        "flame_detected": False,
        "sensor_fault": False,
        "alarm_state": "NORMAL",
        "alarm_latched": False,
        "event": "heartbeat"
    }
    res_hb = client.post("/api/v1/telemetry", headers=auth, json=hb_payload)
    assert res_hb.status_code == 200
    assert res_hb.json() == {"accepted": True, "duplicate": False}

    # Worker runs -> No Telegram message sent for heartbeat
    sent = run_once(temp_db, bot_token="fake", chat_id="123", client=fake_tele)
    assert sent == 0
    assert len(fake_tele.sent_messages) == 0

    # 2. Fire Alarm Started
    alarm_payload = {
        "schema_version": 1,
        "device_id": "kitchen-01",
        "boot_id": "boot-e2e-01",
        "seq": 2,
        "uptime_ms": 2000,
        "sample_age_ms": 0,
        "temperature_c": 65.5,
        "flame_detected": True,
        "sensor_fault": False,
        "alarm_state": "ALARM",
        "alarm_latched": True,
        "event": "alarm_started",
        "incident_id": "inc-e2e-1"
    }
    res_alarm = client.post("/api/v1/telemetry", headers=auth, json=alarm_payload)
    assert res_alarm.status_code == 200
    assert res_alarm.json() == {"accepted": True, "duplicate": False}

    # Worker runs -> 1 Telegram message sent!
    sent = run_once(temp_db, bot_token="fake", chat_id="123", client=fake_tele)
    assert sent == 1
    assert len(fake_tele.sent_messages) == 1
    assert "BÁO CHÁY KHẨN CẤP" in fake_tele.sent_messages[0]["text"]
    assert "inc-e2e-1" in fake_tele.sent_messages[0]["text"]

    # 3. Duplicate re-send of ALARM_STARTED (network glitch)
    res_dup = client.post("/api/v1/telemetry", headers=auth, json=alarm_payload)
    assert res_dup.status_code == 200
    assert res_dup.json() == {"accepted": True, "duplicate": True}

    # Worker runs -> Still no new message!
    sent = run_once(temp_db, bot_token="fake", chat_id="123", client=fake_tele)
    assert sent == 0
    assert len(fake_tele.sent_messages) == 1

    # 4. Fire Cleared
    cleared_payload = {
        "schema_version": 1,
        "device_id": "kitchen-01",
        "boot_id": "boot-e2e-01",
        "seq": 3,
        "uptime_ms": 12000,
        "sample_age_ms": 0,
        "temperature_c": 32.0,
        "flame_detected": False,
        "sensor_fault": False,
        "alarm_state": "NORMAL",
        "alarm_latched": False,
        "event": "alarm_cleared",
        "incident_id": "inc-e2e-1"
    }
    res_clear = client.post("/api/v1/telemetry", headers=auth, json=cleared_payload)
    assert res_clear.status_code == 200
    assert res_clear.json() == {"accepted": True, "duplicate": False}

    # Worker runs -> 1 Telegram clear message sent!
    sent = run_once(temp_db, bot_token="fake", chat_id="123", client=fake_tele)
    assert sent == 1
    assert len(fake_tele.sent_messages) == 2
    assert "BÁO ĐỘNG ĐÃ ĐƯỢC XÓA AN TOÀN" in fake_tele.sent_messages[1]["text"]
