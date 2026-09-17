#!/usr/bin/env bash
set -euo pipefail

SAMPLES="${SAMPLES:-2048}"
REPEATS="${REPEATS:-1000}"
ROUNDS="${ROUNDS:-5}"
XMIN="${XMIN:--10}"
XMAX="${XMAX:-4}"

mkdir -p results
OUT=results/timing.csv
echo "method,scope,samples,repeats,calls,total_ns,ns_per_call" > "$OUT"

mapfile -t METHODS < <(./bin/bench_perf --list)
for scope in kernel e2e; do
  for method in "${METHODS[@]}"; do
    # libm has no separate fixed-point kernel; keep one comparable row in each scope.
    for ((r=1; r<=ROUNDS; r++)); do
      ./bin/bench_perf --method "$method" --scope "$scope" \
        --samples "$SAMPLES" --repeats "$REPEATS" --xmin "$XMIN" --xmax "$XMAX" \
        | tail -n 1 >> "$OUT"
    done
    echo "timed $scope $method" >&2
  done
done

echo "Wrote $OUT"
