#pragma once

#include "market_event.hpp"
#include "matching_engine.hpp"
#include <string>
#include <vector>

namespace me {

// One symbol: one MatchingEngine, one monotonic event sequence (in-memory WAL).
class MarketShard {
    std::string symbol_;
    MatchingEngine engine_;
    uint64_t next_event_seq_{1};
    std::vector<MarketEvent> events_;

public:
    explicit MarketShard(std::string symbol) : symbol_(std::move(symbol)) {}

    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }
    [[nodiscard]] MatchingEngine& engine() noexcept { return engine_; }
    [[nodiscard]] const MatchingEngine& engine() const noexcept { return engine_; }
    [[nodiscard]] uint64_t last_event_seq() const noexcept {
        return events_.empty() ? 0 : events_.back().seq;
    }

    [[nodiscard]] HotResult place_order(PlaceRequest&& req) noexcept;
    [[nodiscard]] HotResult cancel_order(uint64_t order_id) noexcept;
    [[nodiscard]] HotResult query_top() noexcept;
    [[nodiscard]] HotResult query_order(uint64_t order_id) noexcept;

    [[nodiscard]] const std::vector<MarketEvent>& events() const noexcept { return events_; }
    [[nodiscard]] std::vector<MarketEvent> events_since(uint64_t from_seq, std::size_t limit) const;
};

} // namespace me
