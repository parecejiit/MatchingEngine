# Matching Engine Writeup

## Build & run

```bash
export VCPKG_ROOT=/path/to/matchingEngine/vcpkg   # or your global vcpkg
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/matching_engine tests/basic.txt          # scripted replay
INPUT_FILE=tests/basic.txt ./build/matching_engine
./tests/run_tests.sh          # replay + verify_tests.sh assertions
./tests/verify_tests.sh       # substring checks on golden behaviors
./build/me_benchmark 100000   # hot-path latency (no JSON)
```

Interactive: run without args; send one JSON command per line (Boost.Asio async stdin, single thread).

### JSON order format (CLI)

```json
{"cmd": "order", "account_id": 1, "side": "buy", "price": 100.0, "size": 10, "tif": "GTC"}
{"cmd": "cancel", "order_id": 1}
{"cmd": "query", "type": "top"}
{"cmd": "query", "type": "full"}
{"cmd": "query", "type": "order", "order_id": 1}
```

- **Limit order**: include numeric `"price"`.
- **Market order**: omit `"price"` **or** set `"price": null` (both sides: buy/sell).
- **Post-only** (optional): `"post_only": true` on limit orders only.

Each response includes `"ok"`, `"fills"` (array), and `"book"` (top of book) where applicable.

### Post-only

Limit orders with `"post_only": true` are rejected if they would cross the opposite best price (`post_only_would_cross`). Non-crossing post-only orders rest as GTC.

## Design

### Hot path (engine)

- **Fixed pools**: `OrderPool` (65536 slots), `LevelPool` (4096 levels), free lists — no `new`/`delete` while matching.
- **Intrusive FIFO** per price level (doubly linked indices) — O(1) cancel, price-time at level.
- **Sorted level chains** per side with cached `best_bid` / `best_ask` — O(1) best price.
- **Open-addressing maps** for `order_id → slot` and `price_ticks → level` (pre-sized arrays, probe only).
- **int64 price ticks** (1e-8 scale) — no float on hot path; doubles only at JSON boundary.
- **Move-only `OrderHandle`**; fills written into `std::array<Fill, N>` (no vector growth on hot path).
- **Boost.Signals2** `on_fill` for optional subscribers (cold wiring).

### Cold path

- `IoAdapter`: Boost.JSON parse/serialize, string I/O only here.

### Concurrency

- **Single-threaded**: no mutexes in the engine.
- **Boost.Asio** `io_context(1)` for async stdin; matching runs on the same thread.
- Scripted replay (`argv` / `INPUT_FILE`) is synchronous for tests.

## Invariants

1. **Price-time**: better price first; FIFO within level via intrusive queue.
2. **Determinism**: monotonic `order_id` and `timestamp` assigned by engine only.
3. **Self-trade**: same `account_id` → **cancel newest (taker)**; taker rejected, no match.
4. **Partial GTC**: maker partial keeps queue position; taker remainder rests at tail of its limit level.
5. **IOC**: match available liquidity; unfilled remainder discarded (slot freed).
6. **Post-only**: no immediate cross; reject entire order if it would take.

## Tradeoffs & skipped scope

| Chosen | Skipped (per brief) |
|--------|---------------------|
| Pools + intrusive lists | HTTP/WebSocket, auth, persistence |
| Tick prices | Decimal arbitrary precision beyond 1e-8 scale |
| Linear-probe tick map (8192) | Multi-market sharding |
| JSON CLI | Recovery / collateral |

**Why not `std::map`?** Tree nodes heap-allocate; violates hot-path alloc budget.

**Cancel-during-match**: the engine is single-threaded (one command at a time). “Cancel during match” is modeled as: partial fill → cancel resting remainder before the next aggressive order (`cancel_during_match.txt`). There is no concurrent cancel inside the matching loop.

## Tests

| File | Covers |
|------|--------|
| `basic.txt` | Cross, IOC, cancel, self-trade |
| `price_time.txt` | FIFO at same price |
| `partial_fill.txt` | Partial maker + complete |
| `ioc.txt` | No-match IOC, partial IOC |
| `post_only.txt` | Reject cross / rest away |
| `cancel_edge.txt` | Cancel partial remainder, unknown id |
| `market.txt` | Market buy/sell (omit price and `null`) |
| `cancel_during_match.txt` | Cancel resting order after partial fill |

`verify_tests.sh` asserts JSON substrings for deterministic behaviors.

## Time

~2h scope: core book + CLI + tests + this writeup.
