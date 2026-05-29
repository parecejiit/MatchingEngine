#!/bin/bash
# Run replay tests and check critical JSON substrings per suite.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/matching_engine"

if [[ ! -x "$BIN" ]]; then
  echo "Build first: cmake -B build -S . && cmake --build build"
  exit 1
fi

run() {
  "$BIN" "$1"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local label="$3"
  if [[ "$haystack" != *"$needle"* ]]; then
    echo "FAIL: $label"
    echo "  expected substring: $needle"
    echo "  output: $haystack"
    exit 1
  fi
}

echo "==> price_time"
out=$(run "$ROOT/tests/price_time.txt")
assert_contains "$out" '"maker_order_id":1' "fifo first maker"
assert_contains "$out" '"maker_order_id":2' "fifo second maker"
# First fill line should list maker 1 before maker 2 in the sell response
line=$(echo "$out" | sed -n '4p')
assert_contains "$line" '"maker_order_id":1' "first fill is order 1"
echo "OK price_time"

echo "==> partial_fill"
out=$(run "$ROOT/tests/partial_fill.txt")
assert_contains "$out" '"remaining":7' "partial leaves 7 on book"
assert_contains "$out" '"size":7' "second sell completes maker"
echo "OK partial_fill"

echo "==> post_only"
out=$(run "$ROOT/tests/post_only.txt")
assert_contains "$out" '"error":"post_only_would_cross"' "crossing post-only rejected"
assert_contains "$out" '"order_id":3' "non-crossing post-only rests"
assert_contains "$out" '"ask":1.01' "ask at 101 on book"
echo "OK post_only"

echo "==> cancel_edge"
out=$(run "$ROOT/tests/cancel_edge.txt")
assert_contains "$out" '"ok":false' "cancelled order not found"
assert_contains "$out" '"ok":false' "unknown cancel rejected"
echo "OK cancel_edge"

echo "==> ioc"
out=$(run "$ROOT/tests/ioc.txt")
assert_contains "$out" '"fills":[]' "ioc no cross empty fills"
assert_contains "$out" '"size":5' "ioc partial against bid"
assert_contains "$out" '"ok":false' "ioc taker not resting"
echo "OK ioc"

echo "==> basic"
out=$(run "$ROOT/tests/basic.txt")
assert_contains "$out" '"error":"self_trade"' "self-trade rejected"
echo "OK basic"

echo "==> market"
out=$(run "$ROOT/tests/market.txt")
assert_contains "$out" '"taker_order_id":2' "market buy no price field"
assert_contains "$out" '"size":4' "market buy fill size"
assert_contains "$out" '"taker_order_id":3' "market buy price null"
assert_contains "$out" '"taker_order_id":5' "market sell no price field"
assert_contains "$out" '"price":9.9E1' "market sell hits best bid"
echo "OK market"

echo "==> cancel_during_match"
out=$(run "$ROOT/tests/cancel_during_match.txt")
assert_contains "$out" '"remaining":7' "resting after partial before cancel"
assert_contains "$out" '"ok":true' "cancel accepted"
assert_contains "$out" '"ok":false' "query after cancel fails"
assert_contains "$out" '"fills":[]' "no liquidity after cancel"
echo "OK cancel_during_match"

if [[ -x "$ROOT/tests/gen_depth_tests.sh" ]]; then
  "$ROOT/tests/gen_depth_tests.sh"
fi

count_json_levels() {
  echo "$1" | grep -c '"level":' || true
}

echo "==> depth_10"
out=$(run "$ROOT/tests/depth_10.txt")
n=$(count_json_levels "$out")
if [[ "$n" -lt 20 ]]; then
  echo "FAIL: depth_10 expected >= 20 level entries, got $n"
  exit 1
fi
assert_contains "$out" '"bid_levels"' "bid_levels in response"
assert_contains "$out" '"ask_levels"' "ask_levels in response"
echo "OK depth_10 ($n level fields)"

echo "==> depth_20"
out=$(run "$ROOT/tests/depth_20.txt")
n=$(count_json_levels "$out")
if [[ "$n" -lt 40 ]]; then
  echo "FAIL: depth_20 expected >= 40 level entries, got $n"
  exit 1
fi
echo "OK depth_20 ($n level fields)"

echo "==> depth_50"
out=$(run "$ROOT/tests/depth_50.txt")
n=$(count_json_levels "$out")
if [[ "$n" -lt 100 ]]; then
  echo "FAIL: depth_50 expected >= 100 level entries, got $n"
  exit 1
fi
assert_contains "$out" '"level":50' "level 50 on book"
echo "OK depth_50 ($n level fields)"

echo "==> multi_market"
out=$(run "$ROOT/tests/multi_market.txt")
assert_contains "$out" '"symbol":"BTC"' "BTC symbol in response"
assert_contains "$out" '"symbol":"CL"' "CL symbol in response"
assert_contains "$out" '"bid":7.5E1' "CL book unchanged after BTC match"
assert_contains "$out" '"event_seq"' "per-market event sequence"
assert_contains "$out" '"kind":"fill"' "event stream contains fill"
echo "OK multi_market"

echo "==> determinism (replay identity)"
if [[ -x "$ROOT/tests/test_determinism.sh" ]]; then
  ME_BIN="$BIN" "$ROOT/tests/test_determinism.sh"
else
  echo "SKIP test_determinism.sh (not executable)"
fi

echo "All verification checks passed."
