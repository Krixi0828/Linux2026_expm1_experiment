#!/usr/bin/env bash
set -euo pipefail

STEP="${STEP:-0.001}"
XMIN="${XMIN:--10}"
XMAX="${XMAX:-4}"

make -j
mkdir -p results plots
./scripts/save_environment.sh >/dev/null

./bin/accuracy --xmin "$XMIN" --xmax "$XMAX" --step "$STEP" > results/accuracy.csv
./scripts/run_timing.sh
python3 scripts/analyze.py

if [[ "${RUN_PERF:-0}" == "1" ]]; then
  ./scripts/run_perf.sh
  python3 scripts/parse_perf.py || true
fi

echo
printf 'Done. Main outputs:\n  results/summary.csv\n  results/summary.md\n  results/findings.md\n  plots/*.png\n'
