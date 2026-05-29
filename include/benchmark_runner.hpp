#pragma once

#include "matching_engine.hpp"
#include <cstdint>
#include <string>

namespace me {

struct BenchmarkReport {
    std::string name;
    std::uint64_t iterations{0};
    double total_ns{0};
    double min_ns{0};
    double p50_ns{0};
    double p99_ns{0};
    double max_ns{0};
};

// Hot-path only: no JSON, no signals. Returns one report per scenario.
[[nodiscard]] std::string run_benchmarks(std::uint64_t iterations = 500'000);

} // namespace me
