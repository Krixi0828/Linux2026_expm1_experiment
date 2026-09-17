#!/usr/bin/env bash
set -euo pipefail

if ! command -v perf >/dev/null 2>&1; then
  echo "perf not found. Install linux-tools for your kernel, then rerun." >&2
  exit 1
fi

SAMPLES="${SAMPLES:-2048}"
REPEATS="${REPEATS:-1000}"
PERF_REPEATS="${PERF_REPEATS:-5}"
SCOPE="${SCOPE:-kernel}"
mkdir -p results/perf

mapfile -t METHODS < <(./bin/bench_perf --list)
for method in "${METHODS[@]}"; do
  out="results/perf/${SCOPE}_${method}.csv"
  echo "# method=$method scope=$SCOPE" > "$out"
  # perf writes statistics to stderr. -x, asks for machine-readable CSV-like output.
  perf stat -x, -r "$PERF_REPEATS" \
    -e cycles,instructions,branches,branch-misses,cache-references,cache-misses \
    ./bin/bench_perf --method "$method" --scope "$SCOPE" \
      --samples "$SAMPLES" --repeats "$REPEATS" \
      >/dev/null 2>> "$out" || true
  echo "wrote $out" >&2
done
