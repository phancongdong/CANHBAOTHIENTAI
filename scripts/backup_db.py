#!/usr/bin/env python3
"""
Safe Online SQLite Backup Tool for Fire Alarm Database
"""

import argparse
import os
import shutil
import sqlite3
import sys
import tempfile
from pathlib import Path

def backup_database(db_path: str, output_path: str):
    source_p = Path(db_path).resolve()
    target_p = Path(output_path).resolve()

    if not source_p.exists():
        print(f"[ERROR] Source database does not exist: {source_p}")
        sys.exit(1)

    target_p.parent.mkdir(parents=True, exist_ok=True)

    # Use a temporary file in the same directory for atomic rename
    with tempfile.NamedTemporaryFile(dir=target_p.parent, prefix="backup_tmp_", suffix=".db", delete=False) as tmp_f:
        temp_target_path = tmp_f.name

    try:
        print(f"[*] Starting hot online backup: {source_p} -> {temp_target_path}")
        source_conn = sqlite3.connect(f"file:{source_p}?mode=ro", uri=True)
        dest_conn = sqlite3.connect(temp_target_path)

        with dest_conn:
            source_conn.backup(dest_conn)

        source_conn.close()
        dest_conn.close()

        # Run integrity check on the newly backed up file
        print("[*] Running PRAGMA integrity_check on the backup...")
        check_conn = sqlite3.connect(temp_target_path)
        cursor = check_conn.cursor()
        cursor.execute("PRAGMA integrity_check;")
        result = cursor.fetchone()
        check_conn.close()

        if not result or result[0] != "ok":
            print(f"[FATAL] Integrity check failed! Result: {result}")
            if os.path.exists(temp_target_path):
                os.remove(temp_target_path)
            sys.exit(1)

        print("[OK] Integrity check passed: 'ok'")

        # Atomic rename
        shutil.move(temp_target_path, target_p)
        print(f"[SUCCESS] Backup successfully created at: {target_p}")

    except Exception as e:
        print(f"[ERROR] Backup failed: {e}")
        if os.path.exists(temp_target_path):
            os.remove(temp_target_path)
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Perform a safe online backup of the SQLite database")
    parser.add_argument("--database", required=True, help="Path to live SQLite database")
    parser.add_argument("--output", required=True, help="Target backup file path")
    args = parser.parse_args()

    backup_database(args.database, args.output)

if __name__ == "__main__":
    main()
