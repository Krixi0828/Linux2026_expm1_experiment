#!/usr/bin/env python3
from pathlib import Path
import csv
import re

PERF_DIR = Path("results/perf")
OUT = Path("results/perf_summary.csv")

rows = []
if not PERF_DIR.exists():
    raise SystemExit("results/perf does not exist; run scripts/run_perf.sh first")

for path in sorted(PERF_DIR.glob("*.csv")):
    stem = path.stem
    if "_" not in stem:
        continue
    scope, method = stem.split("_", 1)
    for raw in path.read_text(errors="ignore").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
            continue
        value_raw = parts[0].replace(" ", "")
        event = parts[2]
        if event not in {"cycles", "instructions", "branches", "branch-misses", "cache-references", "cache-misses"}:
            continue
        if "not" in value_raw.lower() or "supported" in value_raw.lower():
            value = ""
        else:
            value_clean = value_raw.replace("<", "").replace(">", "")
            try:
                value = float(value_clean)
            except ValueError:
                continue
        stddev_pct = ""
        for p in parts[3:]:
            m = re.search(r"([0-9.]+)%", p)
            if m:
                stddev_pct = float(m.group(1))
                break
        rows.append({
            "method": method,
            "scope": scope,
            "event": event,
            "value": value,
            "stddev_percent": stddev_pct,
            "source": str(path),
        })

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["method", "scope", "event", "value", "stddev_percent", "source"])
    w.writeheader()
    w.writerows(rows)

print(f"Wrote {OUT} with {len(rows)} rows")
