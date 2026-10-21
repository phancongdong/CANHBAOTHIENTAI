import time
import signal
import sys
from datetime import datetime, timezone
import logging

from app.config import (
    DATABASE_PATH,
    TELEGRAM_BOT_TOKEN,
    TELEGRAM_CHAT_ID,
    OFFLINE_TIMEOUT_SECONDS,
    ALARM_REMINDER_INTERVAL_SECONDS
)
from app.db import init_db, get_connection
from app.worker import run_once
from app.monitor import monitor_liveness, monitor_reminders

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("worker")

running = True

def handle_signal(sig, frame):
    global running
    logger.info("Received termination signal, shutting down worker gracefully...")
    running = False

signal.signal(signal.SIGINT, handle_signal)
signal.signal(signal.SIGTERM, handle_signal)

def main():
    logger.info("Starting Telegram Notification Worker and Liveness Monitor...")
    init_db(DATABASE_PATH)

    last_monitor_check = 0.0

    while running:
        now_ts = time.time()
        now_dt = datetime.now(timezone.utc)

        # 1. Process Outbox Rows
        try:
            sent_count = run_once(
                db_path=DATABASE_PATH,
                bot_token=TELEGRAM_BOT_TOKEN,
                chat_id=TELEGRAM_CHAT_ID,
                now=now_dt
            )
            if sent_count > 0:
                logger.info(f"Delivered {sent_count} Telegram notification(s).")
        except Exception as e:
            logger.error(f"Error in outbox processing: {e}")

        # 2. Check Device Liveness & Reminders every 10 seconds
        if now_ts - last_monitor_check >= 10.0:
            last_monitor_check = now_ts
            try:
                conn = get_connection(DATABASE_PATH)
                try:
                    offline_count = monitor_liveness(conn, now_dt, timeout_seconds=OFFLINE_TIMEOUT_SECONDS)
                    if offline_count > 0:
                        logger.warning(f"Marked {offline_count} device(s) as OFFLINE.")

                    reminder_count = monitor_reminders(conn, now_dt, reminder_interval_seconds=ALARM_REMINDER_INTERVAL_SECONDS)
                    if reminder_count > 0:
                        logger.warning(f"Queued {reminder_count} fire alarm reminder(s).")
                finally:
                    conn.close()
            except Exception as e:
                logger.error(f"Error during monitor check: {e}")

        time.sleep(2)

    logger.info("Worker stopped.")

if __name__ == "__main__":
    main()
