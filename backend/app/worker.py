import time
import json
from datetime import datetime, timezone, timedelta
from typing import Optional
import sqlite3
import httpx

from .config import TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, DATABASE_PATH
from .db import get_connection, transaction

class TelegramResult:
    def __init__(
        self,
        success: bool,
        permanent_failure: bool = False,
        retry_after: Optional[int] = None,
        error_msg: Optional[str] = None
    ):
        self.success = success
        self.permanent_failure = permanent_failure
        self.retry_after = retry_after
        self.error_msg = error_msg

def send_telegram(
    bot_token: str,
    chat_id: str,
    text: str,
    client: Optional[httpx.Client] = None
) -> TelegramResult:
    url = f"https://api.telegram.org/bot{bot_token}/sendMessage"
    payload = {
        "chat_id": chat_id,
        "text": text,
        "parse_mode": "HTML",
        "disable_web_page_preview": True
    }

    own_client = False
    if client is None:
        client = httpx.Client(timeout=10.0)
        own_client = True

    try:
        resp = client.post(url, json=payload)
        if resp.status_code == 200:
            return TelegramResult(success=True)

        # Rate Limit handling (429)
        if resp.status_code == 429:
            try:
                data = resp.json()
                retry_after = data.get("parameters", {}).get("retry_after", 5)
            except Exception:
                retry_after = 5
            return TelegramResult(
                success=False,
                permanent_failure=False,
                retry_after=retry_after,
                error_msg=f"Rate limited (429). Retry after {retry_after}s"
            )

        # Permanent authentication / client error
        if resp.status_code in (400, 401, 403, 404):
            return TelegramResult(
                success=False,
                permanent_failure=True,
                error_msg=f"Permanent Telegram HTTP {resp.status_code}: {resp.text}"
            )

        # Server error (5xx)
        return TelegramResult(
            success=False,
            permanent_failure=False,
            error_msg=f"Telegram Server Error {resp.status_code}: {resp.text}"
        )
    except httpx.RequestError as exc:
        return TelegramResult(
            success=False,
            permanent_failure=False,
            error_msg=f"Network exception connecting to Telegram ({type(exc).__name__}): {str(exc)}"
        )
    finally:
        if own_client:
            client.close()

def claim_outbox(
    conn: sqlite3.Connection,
    now: datetime,
    lease_seconds: int = 30,
    limit: int = 10
) -> list[sqlite3.Row]:
    now_iso = now.isoformat()
    lease_until_iso = (now + timedelta(seconds=lease_seconds)).isoformat()

    with transaction(conn):
        cursor = conn.cursor()
        cursor.execute(
            """
            SELECT id, notification_key, device_id, event_type, incident_id, message_text, attempts
            FROM outbox
            WHERE status = 'pending'
              AND next_attempt_at <= ?
              AND (lease_until IS NULL OR lease_until < ?)
            ORDER BY id ASC
            LIMIT ?
            """,
            (now_iso, now_iso, limit)
        )
        rows = cursor.fetchall()
        if rows:
            ids = [r["id"] for r in rows]
            placeholders = ",".join("?" for _ in ids)
            cursor.execute(
                f"""
                UPDATE outbox
                SET lease_until = ?
                WHERE id IN ({placeholders})
                """,
                [lease_until_iso] + ids
            )
        return rows

def process_outbox_row(
    conn: sqlite3.Connection,
    row: sqlite3.Row,
    bot_token: str,
    chat_id: str,
    now: datetime,
    client: Optional[httpx.Client] = None
) -> bool:
    res = send_telegram(bot_token, chat_id, row["message_text"], client=client)
    now_iso = now.isoformat()
    cursor = conn.cursor()

    if res.success:
        with transaction(conn):
            cursor.execute(
                """
                UPDATE outbox
                SET status = 'sent', sent_at = ?, lease_until = NULL, last_error = NULL
                WHERE id = ?
                """,
                (now_iso, row["id"])
            )
        return True
    elif res.permanent_failure:
        with transaction(conn):
            cursor.execute(
                """
                UPDATE outbox
                SET status = 'failed', lease_until = NULL, last_error = ?
                WHERE id = ?
                """,
                (res.error_msg, row["id"])
            )
        return False
    else:
        # Retryable failure
        attempts = row["attempts"] + 1
        if res.retry_after:
            delay_sec = res.retry_after
        else:
            delay_sec = min(300, 2 ** min(attempts, 8))

        next_attempt = (now + timedelta(seconds=delay_sec)).isoformat()
        with transaction(conn):
            cursor.execute(
                """
                UPDATE outbox
                SET attempts = ?, next_attempt_at = ?, lease_until = NULL, last_error = ?
                WHERE id = ?
                """,
                (attempts, next_attempt, res.error_msg, row["id"])
            )
        return False

def run_once(
    db_path: str,
    bot_token: str = TELEGRAM_BOT_TOKEN,
    chat_id: str = TELEGRAM_CHAT_ID,
    now: Optional[datetime] = None,
    client: Optional[httpx.Client] = None
) -> int:
    if now is None:
        now = datetime.now(timezone.utc)

    conn = get_connection(db_path)
    try:
        rows = claim_outbox(conn, now)
        sent_count = 0
        for r in rows:
            if process_outbox_row(conn, r, bot_token, chat_id, now, client=client):
                sent_count += 1
        return sent_count
    finally:
        conn.close()
