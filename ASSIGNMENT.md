# Assignment Coverage (Part 1)

Checklist against the Infinite Worlds exchange take-home (single-market matching engine).

| Requirement | Implementation |
|-------------|----------------|
| Place limit orders (both sides) | `MatchingEngine::place_order` + JSON CLI |
| Place market orders (both sides) | Omit `price` or `"price": null` — see `tests/market.txt` |
| Cancel by order id | `cancel` command |
| Query top of book | `query` / `type: top` |
| Query full depth | `query` / `type: full` |
| Query order status | `query` / `type: order` |
| GTC / IOC | `tif` field |
| Post-only (nice-to-have) | `post_only: true` |
| Price-time priority | Intrusive FIFO + sorted levels |
| Self-trade prevention | Cancel newest (taker) — `WRITEUP.md` |
| Partial fills | Maker keeps queue position |
| Deterministic output | Engine-owned ids/timestamps |
| Single-threaded actor | No locks; sequential replay |
| JSON CLI + scripted replay | `matching_engine`, `tests/*.txt` |
| Writeup | `WRITEUP.md` |
| Git repo | Initialized with `.gitignore` |

**Explicitly out of scope (per brief):** auth, persistence, recovery, collateral, HTTP/WebSocket.

**Run all assignment tests:**

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./tests/verify_tests.sh
```
