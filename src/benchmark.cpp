#include "benchmark_runner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace me {
namespace {

using Clock = std::chrono::steady_clock;

struct Sample {
    double ns{0};
};

static double to_ns(Clock::duration d) {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
}

static BenchmarkReport summarize(const std::string& name, std::uint64_t n,
                                 std::vector<Sample>& samples) {
    std::sort(samples.begin(), samples.end(),
              [](const Sample& a, const Sample& b) { return a.ns < b.ns; });
    BenchmarkReport r{};
    r.name = name;
    r.iterations = n;
    double sum = 0;
    for (const auto& s : samples) sum += s.ns;
    r.total_ns = sum;
    r.min_ns = samples.front().ns;
    r.max_ns = samples.back().ns;
    r.p50_ns = samples[samples.size() / 2].ns;
    r.p99_ns = samples[static_cast<std::size_t>(samples.size() * 99 / 100)].ns;
    return r;
}

static void bench_place_rest_cancel(MatchingEngine& eng, std::uint64_t n,
                                    std::vector<Sample>& out) {
    out.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto t0 = Clock::now();
        PlaceRequest req{};
        req.account_id = 1 + (i % 4);
        req.side = Side::Buy;
        req.limit_ticks = price_to_ticks(99.0 - static_cast<double>(i % 3));
        req.size = 1;
        req.tif = TimeInForce::GTC;
        HotResult placed = eng.place_order(std::move(req));
        const uint64_t oid = placed.placed_order_id;
        (void)eng.cancel_order(oid);
        const auto t1 = Clock::now();
        out.push_back({to_ns(t1 - t0)});
    }
}

static void bench_match_ioc(MatchingEngine& eng, std::uint64_t n, std::vector<Sample>& out) {
    // Seed book once outside timed region.
    (void)eng.place_order(PlaceRequest{.account_id = 10, .side = Side::Sell, .limit_ticks = price_to_ticks(100.0),
                                       .size = 1'000'000, .tif = TimeInForce::GTC});
    out.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto t0 = Clock::now();
        PlaceRequest req{.account_id = 20, .side = Side::Buy, .limit_ticks = price_to_ticks(100.0), .size = 1,
                         .tif = TimeInForce::IOC};
        (void)eng.place_order(std::move(req));
        const auto t1 = Clock::now();
        out.push_back({to_ns(t1 - t0)});
    }
}

static void bench_post_only_reject(MatchingEngine& eng, std::uint64_t n,
                                   std::vector<Sample>& out) {
    (void)eng.place_order(PlaceRequest{.account_id = 30, .side = Side::Buy, .limit_ticks = price_to_ticks(100.0),
                                       .size = 100, .tif = TimeInForce::GTC});
    out.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto t0 = Clock::now();
        PlaceRequest req{.account_id = 31, .side = Side::Sell, .limit_ticks = price_to_ticks(100.0), .size = 1,
                         .tif = TimeInForce::GTC, .post_only = true};
        (void)eng.place_order(std::move(req));
        const auto t1 = Clock::now();
        out.push_back({to_ns(t1 - t0)});
    }
}

static std::string format_report(const BenchmarkReport& r) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1);
    os << r.name << "  n=" << r.iterations << "  min=" << r.min_ns << "ns"
       << "  p50=" << r.p50_ns << "ns  p99=" << r.p99_ns << "ns  max=" << r.max_ns << "ns"
       << "  avg=" << (r.total_ns / static_cast<double>(r.iterations)) << "ns";
    return os.str();
}

} // namespace

std::string run_benchmarks(std::uint64_t iterations) {
    std::ostringstream report;
    report << "Matching engine hot-path benchmark (no JSON, Release recommended)\n";

    {
        MatchingEngine eng;
        std::vector<Sample> samples;
        bench_place_rest_cancel(eng, iterations, samples);
        report << format_report(summarize("place+cancel", iterations, samples)) << '\n';
    }
    {
        MatchingEngine eng;
        std::vector<Sample> samples;
        bench_match_ioc(eng, iterations, samples);
        report << format_report(summarize("IOC match 1 lot", iterations, samples)) << '\n';
    }
    {
        MatchingEngine eng;
        std::vector<Sample> samples;
        bench_post_only_reject(eng, iterations, samples);
        report << format_report(summarize("post-only reject", iterations, samples)) << '\n';
    }
    return report.str();
}

} // namespace me

int main(int argc, char* argv[]) {
    std::uint64_t n = 500'000;
    if (argc > 1) n = static_cast<std::uint64_t>(std::stoull(argv[1]));
    std::cout << me::run_benchmarks(n);
    return 0;
}
