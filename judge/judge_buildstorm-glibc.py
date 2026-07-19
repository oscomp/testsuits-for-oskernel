#!/usr/bin/env python3
"""BuildStorm judge (contestant-facing reference) -- 180 scriptable pts.


Input : serial log(s) -- file paths as argv, or stdin.
Output: JSON list on stdout, autotest-compatible: [{name, pass, total, score}]
        Human-readable summary on stderr.

Scoring (200 pts total; doc 20 pts judged manually, not here):
  buildstorm env toolchain     8   BUILDSTORM_TOOLCHAIN ok            (binary)
  buildstorm env minibuild    12   BUILDSTORM_MINIBUILD ok            (binary)
  buildstorm compile ok       40   BUILDSTORM_COMPILE mode=multi ok=true
  buildstorm compile time    120   120 * clamp((2*B - t) / B, 0, 1)

Only the multi-core (8c/8G) configuration is scored. Compile time t comes from
the guest-reported elapsed_s (measured in-guest via /proc/uptime); tampering
with the clock or /proc/uptime is treated as cheating (contest rule).
Baseline B (seconds) is the placeholder below -- RE-MEASURE on the judge machine.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

BASELINE = 400.0
try:
    with open(os.path.join(HERE, "config.json")) as f:
        BASELINE = float(json.load(f).get("baseline.multi_s", BASELINE))
except Exception:
    pass

SUCCESS_PTS = 40.0
TIME_PTS = 120.0
ENV_PTS = {"TOOLCHAIN": 8.0, "MINIBUILD": 12.0}
EXPECTED_CORES = 8


def read_input():
    if len(sys.argv) > 1:
        return "".join(open(p, errors="ignore").read() for p in sys.argv[1:])
    return sys.stdin.read()


def time_score(t, baseline):
    """<= B full marks; >= 2B zero; linear in between."""
    return round(TIME_PTS * max(0.0, min(1.0, (2 * baseline - t) / baseline)), 1)


def parse_compile(out):
    """Return the kv dict of the LAST BUILDSTORM_COMPILE line (any mode)."""
    kv = None
    for m in re.finditer(r"BUILDSTORM_COMPILE\s+((?:\S+=\S+[ \t]*)+)", out):
        kv = dict(p.split("=", 1) for p in m.group(1).split())
    return kv


def main():
    out = read_input()
    results = []
    notes = []

    # ---- environment (binary items)
    for tag, pts in ENV_PTS.items():
        ok = bool(re.search(rf"BUILDSTORM_{tag}\s+ok\b", out))
        results.append({"name": f"buildstorm env {tag.lower()}",
                        "pass": 1 if ok else 0, "total": 1,
                        "score": pts if ok else 0})

    # ---- compile run (multi-core only)
    kv = parse_compile(out)
    compile_ok = bool(kv) and kv.get("ok") == "true"
    results.append({"name": "buildstorm compile ok",
                    "pass": 1 if compile_ok else 0, "total": 1,
                    "score": SUCCESS_PTS if compile_ok else 0})

    tscore = 0.0
    if compile_ok:
        try:
            t = float(kv.get("elapsed_s"))
            tscore = time_score(t, BASELINE)
            notes.append(f"compile: elapsed={t:.0f}s baseline={BASELINE:.0f}s")
        except (TypeError, ValueError):
            notes.append("compile: ok=true but elapsed_s missing/bad -> time score 0")
        cores = int(kv.get("cores", 0) or 0)
        if cores != EXPECTED_CORES:
            notes.append(f"WARN: guest saw cores={cores}, expected {EXPECTED_CORES} "
                         f"-- check qemu -smp")
    else:
        notes.append(f"compile: ok={kv.get('ok') if kv else 'missing'}")

    results.append({"name": "buildstorm compile time",
                    "pass": 1 if tscore > 0 else 0, "total": 1,
                    "score": tscore})

    print(json.dumps(results))

    total = sum(r["score"] for r in results)
    print(f"[judge] scripted total = {total:.1f} / 180.0 "
          f"(+ doc 20 pts, judged manually) = 200 ", file=sys.stderr)
    for n in notes:
        print(f"[judge] {n}", file=sys.stderr)


if __name__ == "__main__":
    main()
