import sqlite3
import os
from pathlib import Path
from contextlib import contextmanager

SCHEMA_SQL = """
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;

CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    boot_id TEXT NOT NULL,
    seq INTEGER NOT NULL,
    received_at TIMESTAMP NOT NULL,
    payload TEXT NOT NULL,
    payload_hash TEXT NOT NULL,
    UNIQUE(device_id, boot_id, seq)
);

CREATE TABLE IF NOT EXISTS device_state (
    device_id TEXT PRIMARY KEY,
    last_boot_id TEXT NOT NULL,
    last_seq INTEGER NOT NULL,
    last_seen_at TIMESTAMP NOT NULL,
    current_state TEXT NOT NULL,
    latched INTEGER NOT NULL,
    incident_id TEXT,
    last_temperature REAL,
    sensor_fault INTEGER NOT NULL,
    is_offline INTEGER DEFAULT 0,
    last_alarm_reminded_at TIMESTAMP
);

CREATE TABLE IF NOT EXISTS outbox (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    notification_key TEXT UNIQUE NOT NULL,
    device_id TEXT NOT NULL,
    event_type TEXT NOT NULL,
    incident_id TEXT,
    message_text TEXT NOT NULL,
    payload TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'pending',
    attempts INTEGER NOT NULL DEFAULT 0,
    next_attempt_at TIMESTAMP NOT NULL,
    lease_until TIMESTAMP,
    last_error TEXT,
    sent_at TIMESTAMP,
    created_at TIMESTAMP NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_outbox_pending ON outbox(status, next_attempt_at);
CREATE INDEX IF NOT EXISTS idx_events_lookup ON events(device_id, boot_id, seq);
"""

def init_db(db_path: str):
    p = Path(db_path)
    p.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(db_path) as conn:
        conn.executescript(SCHEMA_SQL)
        conn.commit()

def get_connection(db_path: str) -> sqlite3.Connection:
    conn = sqlite3.connect(db_path, timeout=10.0, isolation_level=None)
    conn.row_factory = sqlite3.Row
    return conn

@contextmanager
def transaction(conn: sqlite3.Connection):
    conn.execute("BEGIN IMMEDIATE")
    try:
        yield conn
        conn.execute("COMMIT")
    except Exception:
        conn.execute("ROLLBACK")
        raise
