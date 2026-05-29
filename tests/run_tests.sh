#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/matching_engine"
if [[ ! -x "$BIN" ]]; then
  echo "Build first: cmake -B build -S . && cmake --build build"
  exit 1
fi
for t in "$ROOT/tests"/*.txt; do
  echo "==> $(basename "$t")"
  INPUT_FILE="$t" "$BIN"
  echo "---"
done
"$ROOT/tests/verify_tests.sh"
