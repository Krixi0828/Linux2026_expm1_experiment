#!/usr/bin/env bash
set -euo pipefail
mkdir -p results
{
  echo '=== date ==='
  date -Is
  echo '=== uname ==='
  uname -a
  echo '=== gcc ==='
  gcc --version | head -n 1
  echo '=== CPU ==='
  lscpu | grep -E 'Architecture|Model name|CPU\(s\)|CPU MHz|CPU max MHz|CPU min MHz' || true
  echo '=== perf ==='
  perf --version 2>/dev/null || true
  echo '=== perf_event_paranoid ==='
  cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || true
} > results/environment.txt
cat results/environment.txt
