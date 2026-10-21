from typing import Optional, Literal
from pydantic import BaseModel, Field

class TelemetryEventIn(BaseModel):
    schema_version: Literal[1] = 1
    device_id: str = Field(..., max_length=64)
    boot_id: str = Field(..., max_length=32)
    seq: int = Field(..., ge=0)
    uptime_ms: int = Field(..., ge=0)
    sample_age_ms: int = Field(..., ge=0)
    temperature_c: Optional[float] = None
    flame_detected: Optional[bool] = None
    sensor_fault: bool
    alarm_state: Literal["NORMAL", "SUSPECT", "ALARM", "SENSOR_FAULT"]
    alarm_latched: bool
    event: Literal["heartbeat", "alarm_started", "alarm_cleared", "sensor_fault", "sensor_recovered"]
    incident_id: Optional[str] = None

class TelemetryResponse(BaseModel):
    accepted: bool
    duplicate: bool

class HealthResponse(BaseModel):
    status: str
    database: str
