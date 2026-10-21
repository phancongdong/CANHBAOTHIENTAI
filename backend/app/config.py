import os
from pathlib import Path
from dotenv import load_dotenv

load_dotenv()

BASE_DIR = Path(__file__).resolve().parent.parent

DATABASE_PATH = os.getenv("DATABASE_PATH", str(BASE_DIR / "data" / "fire_alarm.db"))
DEVICE_CREDENTIALS_FILE = os.getenv("DEVICE_CREDENTIALS_FILE", str(BASE_DIR / "data" / "device_credentials.json"))

TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN", "8622569426:AAGOlFk9Zfhxlj6d-WGSGb9C-GCdcYLM-9U")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID", "6130858384")
PUBLIC_DOMAIN = os.getenv("PUBLIC_DOMAIN", "localhost")

OFFLINE_TIMEOUT_SECONDS = int(os.getenv("OFFLINE_TIMEOUT_SECONDS", "60"))
ALARM_REMINDER_INTERVAL_SECONDS = int(os.getenv("ALARM_REMINDER_INTERVAL_SECONDS", "300"))
