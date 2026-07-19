#!/usr/bin/env python3
"""Contestant-facing CAgent judge."""

import json
import re
import sys


TESTS = {
    "factorial": ("easy", 13.5, 20000),
    "date": ("easy", 13.5, 20000),
    "network": ("medium", 20.0, 25000),
    "cpu": ("easy", 13.5, 20000),
    "kernel": ("easy", 13.5, 20000),
    "fs-create": ("medium", 20.0, 25000),
    "fs-readwrite": ("medium", 20.0, 30000),
    "fs-directory": ("medium", 20.0, 30000),
    "fs-search": ("hard", 27.0, 35000),
    "fs-usage": ("medium", 20.0, 25000),
}


def parse_records(text):
    records = {}
    pattern = re.compile(
        r"testcase\s+cagent\s+(\S+)\s+(pass|reject)\s+(\d+)"
    )
    for match in pattern.finditer(text):
        name, status, elapsed_ms = match.groups()
        if name in TESTS:
            records[name] = {
                "status": status,
                "elapsed_ms": elapsed_ms,
            }
    return records


def main():
    records = parse_records(sys.stdin.read())
    results = []
    for name, (difficulty, weight, timeout_ms) in TESTS.items():
        record = records.get(name, {})
        passed = record.get("status") == "pass"
        try:
            elapsed_ms = int(record.get("elapsed_ms", "0"))
        except ValueError:
            elapsed_ms = 0
        bonus = weight * 0.1 if passed and 0 < elapsed_ms < timeout_ms / 2 else 0
        results.append({
            "name": f"cagent {name}",
            "pass": 1 if passed else 0,
            "total": 1,
            "score": round(weight + bonus, 2) if passed else 0,
            "exec_time_ms": elapsed_ms,
            "difficulty": difficulty,
        })

    print(json.dumps(results))


if __name__ == "__main__":
    main()
