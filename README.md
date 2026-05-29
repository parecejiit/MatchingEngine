# Matching Engine (C++20)

Ultra-low-latency single-market limit order book for an exchange take-home.

## Requirements

- CMake ≥ 3.15, C++20 compiler
- vcpkg (bundled under `vcpkg/` or set `VCPKG_ROOT`)

## Build

```bash
export VCPKG_ROOT="$(pwd)/vcpkg"   # adjust if needed
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
# Scripted replay (deterministic tests)
./build/matching_engine tests/basic.txt

# Or
./tests/run_tests.sh

# Hot-path benchmark (no JSON)
./build/me_benchmark 500000
```

## Agent guidelines

See `.cursor/rules/matching-engine-performance.mdc` — always-on rules for this repo.

## Docs

- [WRITEUP.md](WRITEUP.md) — design, invariants, tradeoffs
- [ASSIGNMENT.md](ASSIGNMENT.md) — Part 1 requirement checklist
