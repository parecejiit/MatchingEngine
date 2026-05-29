#!/bin/bash
# Generate depth_10.txt, depth_20.txt, depth_50.txt with N bid + N ask resting levels.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"

gen_file() {
  local n=$1
  local out="$ROOT/depth_${n}.txt"
  {
    echo "# ${n} bid levels + ${n} ask levels (1 lot each, GTC)"
    local i
    for ((i = 0; i < n; ++i)); do
      local price
      price=$(awk -v i="$i" 'BEGIN { printf "%.2f", 100.00 - i * 0.01 }')
      printf '{"cmd":"order","account_id":%d,"side":"buy","price":%s,"size":1,"tif":"GTC"}\n' \
        "$((i + 1))" "$price"
    done
    for ((i = 0; i < n; ++i)); do
      local price
      price=$(awk -v i="$i" 'BEGIN { printf "%.2f", 100.50 + i * 0.01 }')
      printf '{"cmd":"order","account_id":%d,"side":"sell","price":%s,"size":1,"tif":"GTC"}\n' \
        "$((1000 + i))" "$price"
    done
    echo '{"cmd":"query","type":"top"}'
    echo '{"cmd":"query","type":"full"}'
  } >"$out"
  echo "Wrote $out"
}

gen_file 10
gen_file 20
gen_file 50
