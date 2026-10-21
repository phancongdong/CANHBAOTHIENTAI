#!/usr/bin/env python3
"""
ESP32 Fire Alarm Device Simulator for End-to-End Testing
"""

import argparse
import json
import time
import sys
import httpx

def send_telemetry(client: httpx.Client, base_url: str, token: str, payload: dict) -> dict:
    url = f"{base_url.rstrip('/')}/api/v1/telemetry"
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {token}"
    }
    resp = client.post(url, headers=headers, json=payload, timeout=10.0)
    print(f"[{time.strftime('%H:%M:%S')}] POST {url} (seq={payload.get('seq')}, event={payload.get('event')}) -> HTTP {resp.status_code}")
    if resp.status_code != 200:
        print(f"    Error Response: {resp.text}")
        resp.raise_for_status()
    data = resp.json()
    print(f"    Server Response: {data}")
    return data

def make_event(
    device_id: str,
    boot_id: str,
    seq: int,
    uptime_ms: int,
    temp_c: float | None,
    flame: bool | None,
    fault: bool,
    state: str,
    latched: bool,
    event: str,
    incident_id: str | None = None
) -> dict:
    return {
        "schema_version": 1,
        "device_id": device_id,
        "boot_id": boot_id,
        "seq": seq,
        "uptime_ms": uptime_ms,
        "sample_age_ms": 0,
        "temperature_c": temp_c,
        "flame_detected": flame,
        "sensor_fault": fault,
        "alarm_state": state,
        "alarm_latched": latched,
        "event": event,
        "incident_id": incident_id
    }

def run_alarm_cycle(client: httpx.Client, base_url: str, device_id: str, token: str):
    boot_id = f"sim-{int(time.time())}"
    print(f"\n=== SCENARIO: ALARM CYCLE (Device: {device_id}, Boot: {boot_id}) ===")

    # 1. Normal Heartbeat
    print("\n1. Sending initial normal heartbeat...")
    hb1 = make_event(device_id, boot_id, seq=1, uptime_ms=1000, temp_c=25.5, flame=False, fault=False, state="NORMAL", latched=False, event="heartbeat")
    res1 = send_telemetry(client, base_url, token, hb1)
    assert res1["accepted"] is True and res1["duplicate"] is False

    # 2. Fire detected -> ALARM_STARTED
    print("\n2. Simulating confirmed fire detection (ALARM_STARTED)...")
    alarm = make_event(device_id, boot_id, seq=2, uptime_ms=2000, temp_c=65.2, flame=True, fault=False, state="ALARM", latched=True, event="alarm_started", incident_id="inc-sim-001")
    res2 = send_telemetry(client, base_url, token, alarm)
    assert res2["accepted"] is True and res2["duplicate"] is False

    # 3. Duplicate ALARM_STARTED (network retransmission)
    print("\n3. Testing idempotency: Re-sending exact same ALARM_STARTED payload...")
    res3 = send_telemetry(client, base_url, token, alarm)
    assert res3["accepted"] is True and res3["duplicate"] is True

    # 4. Fire extinguished -> ALARM_CLEARED
    print("\n4. Simulating fire extinguished & cleared (ALARM_CLEARED)...")
    cleared = make_event(device_id, boot_id, seq=3, uptime_ms=7000, temp_c=34.0, flame=False, fault=False, state="NORMAL", latched=False, event="alarm_cleared", incident_id="inc-sim-001")
    res4 = send_telemetry(client, base_url, token, cleared)
    assert res4["accepted"] is True and res4["duplicate"] is False

    # 5. Normal Heartbeat resumed
    print("\n5. Resuming normal heartbeat...")
    hb2 = make_event(device_id, boot_id, seq=4, uptime_ms=37000, temp_c=26.0, flame=False, fault=False, state="NORMAL", latched=False, event="heartbeat")
    res5 = send_telemetry(client, base_url, token, hb2)
    assert res5["accepted"] is True and res5["duplicate"] is False

    print("\n>>> ALARM CYCLE SCENARIO COMPLETED SUCCESSFULLY! <<<\n")

def run_sensor_fault_cycle(client: httpx.Client, base_url: str, device_id: str, token: str):
    boot_id = f"sim-{int(time.time())}"
    print(f"\n=== SCENARIO: SENSOR FAULT CYCLE (Device: {device_id}, Boot: {boot_id}) ===")

    # Fault started
    print("\n1. Simulating sensor disconnect fault...")
    fault = make_event(device_id, boot_id, seq=1, uptime_ms=5000, temp_c=None, flame=None, fault=True, state="SENSOR_FAULT", latched=False, event="sensor_fault")
    res1 = send_telemetry(client, base_url, token, fault)
    assert res1["accepted"] is True

    # Fault recovered
    print("\n2. Simulating sensor recovered...")
    rec = make_event(device_id, boot_id, seq=2, uptime_ms=10000, temp_c=27.0, flame=False, fault=False, state="NORMAL", latched=False, event="sensor_recovered")
    res2 = send_telemetry(client, base_url, token, rec)
    assert res2["accepted"] is True

    print("\n>>> SENSOR FAULT CYCLE COMPLETED SUCCESSFULLY! <<<\n")

def main():
    parser = argparse.ArgumentParser(description="Simulate ESP32 Fire Alarm Device")
    parser.add_argument("--base-url", default="http://127.0.0.1:8000", help="Base URL of backend API")
    parser.add_argument("--device-id", default="kitchen-01", help="Device ID")
    parser.add_argument("--token", default="secret-token-kitchen-01", help="Device Bearer Token")
    parser.add_argument("--scenario", default="alarm-cycle", choices=["alarm-cycle", "sensor-fault"], help="Scenario to run")

    args = parser.parse_args()

    with httpx.Client() as client:
        try:
            health = client.get(f"{args.base_url.rstrip('/')}/healthz", timeout=5.0)
            if health.status_code != 200:
                print(f"[ERROR] Target backend /healthz returned {health.status_code}: {health.text}")
                sys.exit(1)
            print(f"[OK] Connected to backend at {args.base_url}")
        except Exception as e:
            print(f"[ERROR] Cannot connect to {args.base_url}: {e}")
            sys.exit(1)

        if args.scenario == "alarm-cycle":
            run_alarm_cycle(client, args.base_url, args.device_id, args.token)
        elif args.scenario == "sensor-fault":
            run_sensor_fault_cycle(client, args.base_url, args.device_id, args.token)

if __name__ == "__main__":
    main()
