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

echo "All verification checks passed."
