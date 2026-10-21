from datetime import datetime, timezone, timedelta
import httpx
import pytest
from app.db import get_connection
from app.worker import claim_outbox, process_outbox_row, run_once

class FakeResponse:
    def __init__(self, status_code, json_data=None, text=""):
        self.status_code = status_code
        self._json_data = json_data or {}
        self.text = text

    def json(self):
        return self._json_data

class FakeHttpClient:
    def __init__(self, status_code=200, json_data=None, raises_error=False):
        self.status_code = status_code
        self.json_data = json_data or {"ok": True}
        self.raises_error = raises_error
        self.call_count = 0

    def post(self, url, json=None):
        self.call_count += 1
        if self.raises_error:
            raise httpx.ConnectTimeout("Connection timed out")
        return FakeResponse(self.status_code, self.json_data)

def insert_outbox_row(db_path, key="test-key", status="pending", next_attempt=None):
    now_iso = (next_attempt or datetime.now(timezone.utc)).isoformat()
    conn = get_connection(db_path)
    cursor = conn.cursor()
    cursor.execute(
        """
        INSERT INTO outbox (
            notification_key, device_id, event_type, incident_id,
            message_text, payload, status, attempts, next_attempt_at, created_at
        ) VALUES (?, 'kitchen-01', 'alarm_started', 'inc-1', 'Fire Alert!', '{}', ?, 0, ?, ?)
        """,
        (key, status, now_iso, now_iso)
    )
    conn.commit()
    conn.close()

def test_successful_send_marks_sent(temp_db):
    insert_outbox_row(temp_db, key="key-success")
    fake_client = FakeHttpClient(status_code=200, json_data={"ok": True})

    sent = run_once(temp_db, bot_token="fake-token", chat_id="12345", client=fake_client)
    assert sent == 1
    assert fake_client.call_count == 1

    conn = get_connection(temp_db)
    cursor = conn.cursor()
    cursor.execute("SELECT status, sent_at FROM outbox WHERE notification_key = 'key-success'")
    row = cursor.fetchone()
    assert row["status"] == "sent"
    assert row["sent_at"] is not None
    conn.close()

def test_timeout_keeps_pending_with_backoff(temp_db):
    now = datetime.now(timezone.utc)
    insert_outbox_row(temp_db, key="key-timeout", next_attempt=now)
    fake_client = FakeHttpClient(raises_error=True)

    sent = run_once(temp_db, bot_token="fake-token", chat_id="12345", now=now, client=fake_client)
    assert sent == 0

    conn = get_connection(temp_db)
    cursor = conn.cursor()
    cursor.execute("SELECT status, attempts, next_attempt_at, last_error FROM outbox WHERE notification_key = 'key-timeout'")
    row = cursor.fetchone()
    assert row["status"] == "pending"
    assert row["attempts"] == 1
    assert row["next_attempt_at"] > now.isoformat()
    assert "ConnectTimeout" in row["last_error"]
    conn.close()

def test_rate_limit_429_uses_retry_after(temp_db):
    now = datetime.now(timezone.utc)
    insert_outbox_row(temp_db, key="key-429", next_attempt=now)
    fake_client = FakeHttpClient(status_code=429, json_data={"ok": False, "parameters": {"retry_after": 25}})

    sent = run_once(temp_db, bot_token="fake-token", chat_id="12345", now=now, client=fake_client)
    assert sent == 0

    conn = get_connection(temp_db)
    cursor = conn.cursor()
    cursor.execute("SELECT status, next_attempt_at FROM outbox WHERE notification_key = 'key-429'")
    row = cursor.fetchone()
    assert row["status"] == "pending"
    expected_min = (now + timedelta(seconds=24)).isoformat()
    assert row["next_attempt_at"] >= expected_min
    conn.close()

def test_lease_claim_and_expiration(temp_db):
    t0 = datetime.now(timezone.utc)
    insert_outbox_row(temp_db, key="key-lease", next_attempt=t0)
    conn = get_connection(temp_db)

    # First claim with 30s lease
    rows = claim_outbox(conn, t0, lease_seconds=30)
    assert len(rows) == 1

    # Immediate second claim before lease expiry returns nothing
    rows_empty = claim_outbox(conn, t0 + timedelta(seconds=10), lease_seconds=30)
    assert len(rows_empty) == 0

    # Claim after lease expired returns the row again (worker crashed recovery)
    rows_reclaimed = claim_outbox(conn, t0 + timedelta(seconds=35), lease_seconds=30)
    assert len(rows_reclaimed) == 1
    conn.close()
