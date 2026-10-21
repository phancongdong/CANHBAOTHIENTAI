import json
import pytest

def sample_alarm_event():
    return {
        "schema_version": 1,
        "device_id": "kitchen-01",
        "boot_id": "boot-000001",
        "seq": 10,
        "uptime_ms": 15000,
        "sample_age_ms": 0,
        "temperature_c": 56.5,
        "flame_detected": True,
        "sensor_fault": False,
        "alarm_state": "ALARM",
        "alarm_latched": True,
        "event": "alarm_started",
        "incident_id": "inc-000001"
    }

def test_healthz_endpoint(client):
    res = client.get("/healthz")
    assert res.status_code == 200
    assert res.json()["status"] == "ok"

def test_unauthorized_access(client):
    res = client.post("/api/v1/telemetry", json=sample_alarm_event())
    assert res.status_code == 401

def test_forbidden_invalid_token(client):
    res = client.post(
        "/api/v1/telemetry",
        headers={"Authorization": "Bearer wrong-token"},
        json=sample_alarm_event()
    )
    assert res.status_code == 403

def test_invalid_schema_unprocessable(client, auth_headers):
    bad_payload = sample_alarm_event()
    bad_payload["schema_version"] = 2  # Only 1 is allowed
    res = client.post("/api/v1/telemetry", headers=auth_headers, json=bad_payload)
    assert res.status_code == 422

def test_body_size_limit_exceeded(client, auth_headers):
    large_payload = sample_alarm_event()
    large_payload["device_id"] = "a" * 10000  # Exceeds 8 KiB
    res = client.post("/api/v1/telemetry", headers=auth_headers, json=large_payload)
    assert res.status_code == 413
