#!/bin/bash
# Prove deterministic matching: identical byte-for-byte replay output across runs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ME_BIN:-$ROOT/build/matching_engine}"
TMP="${TMPDIR:-/tmp}/me-determinism-$$"
REPEAT="${ME_DETERMINISM_REPEAT:-8}"

mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$BIN" ]]; then
  echo "Build first: cmake -B build -S . && cmake --build build"
  exit 1
fi

run_replay() {
  "$BIN" "$1"
}

sha256_file() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  else
    sha256sum "$1" | awk '{print $1}'
  fi
}

assert_identical_runs() {
  local label="$1"
  local file="$2"
  local a="$TMP/${label}.run1.txt"
  local b="$TMP/${label}.run2.txt"
  run_replay "$file" > "$a"
  run_replay "$file" > "$b"
  if ! cmp -s "$a" "$b"; then
    echo "FAIL determinism (two consecutive runs differ): $label"
    diff -u "$a" "$b" | head -80 || true
    exit 1
  fi
  echo "OK determinism: $label (identical x2)"
}

assert_repeatable() {
  local label="$1"
  local file="$2"
  local n="$3"
  local ref="$TMP/${label}.ref.txt"
  run_replay "$file" > "$ref"
  local i
  for ((i = 2; i <= n; i++)); do
    local cur="$TMP/${label}.run${i}.txt"
    run_replay "$file" > "$cur"
    if ! cmp -s "$ref" "$cur"; then
      echo "FAIL determinism (run $i differs from run 1): $label"
      diff -u "$ref" "$cur" | head -80 || true
      exit 1
    fi
  done
  echo "OK determinism: $label (identical x${n})"
}

assert_golden_sha256() {
  local label="$1"
  local file="$2"
  local golden="$ROOT/tests/${label}.sha256"
  local out="$TMP/${label}.out.txt"
  if [[ ! -f "$golden" ]]; then
    echo "SKIP golden hash (missing $golden)"
    return 0
  fi
  run_replay "$file" > "$out"
  local expected actual
  expected=$(awk '{print $1}' "$golden")
  actual=$(sha256_file "$out")
  if [[ "$expected" != "$actual" ]]; then
    echo "FAIL golden hash mismatch: $label"
    echo "  expected: $expected"
    echo "  actual:   $actual"
    exit 1
  fi
  echo "OK determinism: $label (golden sha256)"
}

assert_stress_invariants() {
  local file="$1"
  local out
  out=$(<"$file")
  # FIFO at 100: first fill uses maker order_id 1 (comma after id avoids matching :10, :11, …)
  if [[ "$out" != *'"maker_order_id":1,'* ]]; then
    echo "FAIL stress invariant: expected maker_order_id 1 in IOC sell"
    exit 1
  fi
  if [[ "$out" != *'"error":"post_only_would_cross"'* ]]; then
    echo "FAIL stress invariant: post-only cross reject"
    exit 1
  fi
  if [[ "$out" != *'"error":"self_trade"'* ]]; then
    echo "FAIL stress invariant: self-trade reject"
    exit 1
  fi
  if [[ "$out" != *'"taker_order_id":16'* ]]; then
    echo "FAIL stress invariant: market sell taker id 16"
    exit 1
  fi
  echo "OK determinism_stress invariants"
}

compare_bins_if_present() {
  local alt="${ME_BIN_O0:-$ROOT/build-verify/matching_engine}"
  local stress="$ROOT/tests/determinism_stress.txt"
  if [[ ! -x "$alt" ]]; then
    echo "SKIP O3 vs O0 (no binary at $alt; build with -DME_DISABLE_COMPILER_OPTIMIZATION=ON)"
    return 0
  fi
  local o3="$TMP/stress.o3.txt"
  local o0="$TMP/stress.o0.txt"
  "$BIN" "$stress" > "$o3"
  "$alt" "$stress" > "$o0"
  if ! cmp -s "$o3" "$o0"; then
    echo "FAIL determinism: Release (-O3) vs ME_DISABLE_COMPILER_OPTIMIZATION (-O0) differ"
    diff -u "$o3" "$o0" | head -80 || true
    exit 1
  fi
  echo "OK determinism: -O3 vs -O0 output identical (determinism_stress.txt)"
}

echo "==> determinism: all replay fixtures (2 runs each)"
shopt -s nullglob
for t in "$ROOT/tests"/*.txt; do
  base=$(basename "$t")
  assert_identical_runs "${base%.txt}" "$t"
done

echo "==> determinism: stress stream (${REPEAT} runs)"
assert_repeatable "determinism_stress" "$ROOT/tests/determinism_stress.txt" "$REPEAT"
stress_out="$TMP/determinism_stress.invariants.txt"
run_replay "$ROOT/tests/determinism_stress.txt" > "$stress_out"
assert_stress_invariants "$stress_out"

echo "==> determinism: committed golden sha256"
assert_golden_sha256 "determinism_stress" "$ROOT/tests/determinism_stress.txt"

echo "==> determinism: optimized vs no-optimization binary (optional)"
compare_bins_if_present

echo "All determinism checks passed."
