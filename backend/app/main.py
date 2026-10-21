import json
import os
import hmac
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, Header, HTTPException, Request, Depends, status
from fastapi.responses import JSONResponse
from contextlib import asynccontextmanager

from .config import DATABASE_PATH, DEVICE_CREDENTIALS_FILE
from .models import TelemetryEventIn, TelemetryResponse, HealthResponse
from .db import init_db, get_connection
from .ingest import ingest_event

def load_credentials() -> dict[str, str]:
    cred_file = Path(DEVICE_CREDENTIALS_FILE)
    if cred_file.exists():
        try:
            with open(cred_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    # Default development/test credentials
    return {
        "kitchen-01": "secret-token-kitchen-01",
        "test-device": "test-token"
    }

@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db(DATABASE_PATH)
    yield

app = FastAPI(
    title="ESP32 Fire Alarm Telemetry API",
    version="1.0.0",
    lifespan=lifespan
)

def verify_token(authorization: Optional[str] = Header(None)) -> str:
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing or invalid Bearer authorization header"
        )
    token = authorization.split("Bearer ", 1)[1].strip()
    return token

@app.get("/healthz", response_model=HealthResponse)
def healthz():
    try:
        conn = get_connection(DATABASE_PATH)
        cursor = conn.cursor()
        cursor.execute("SELECT 1")
        cursor.fetchone()
        conn.close()
        return HealthResponse(status="ok", database="connected")
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=f"Database unreachable: {str(e)}"
        )

@app.post("/api/v1/telemetry", response_model=TelemetryResponse)
async def post_telemetry(
    request: Request,
    token: str = Depends(verify_token)
):
    # Enforce payload body size limit (8 KiB)
    body_bytes = await request.body()
    if len(body_bytes) > 8192:
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail="Payload exceeds 8 KiB limit"
        )

    try:
        payload_str = body_bytes.decode("utf-8")
        data = json.loads(payload_str)
        event = TelemetryEventIn(**data)
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail=f"Invalid telemetry schema: {str(e)}"
        )

    # Validate device token
    credentials = load_credentials()
    expected_token = credentials.get(event.device_id)
    if not expected_token or not hmac.compare_digest(token, expected_token):
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Device token mismatch or unauthorized device"
        )

    now = datetime.now(timezone.utc)
    conn = get_connection(DATABASE_PATH)
    try:
        response = ingest_event(conn, event, payload_str, now)
        return response
    finally:
        conn.close()
