#pragma once

#include "market_shard.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace me {

struct RoutedResult {
    HotResult hot{};
    MarketShard* shard{nullptr};
    bool named_market{false}; // true when client supplied "symbol"
};

// Routes commands to independent per-symbol shards (each owns an engine + event log).
class MarketRouter {
    std::unordered_map<std::string, std::unique_ptr<MarketShard>> shards_;

public:
    [[nodiscard]] MarketShard& shard(std::string_view symbol);
    [[nodiscard]] RoutedResult place_order(std::string_view symbol, PlaceRequest&& req) noexcept;
    [[nodiscard]] RoutedResult cancel_order(std::string_view symbol, uint64_t order_id) noexcept;
    [[nodiscard]] RoutedResult query_top(std::string_view symbol) noexcept;
    [[nodiscard]] RoutedResult query_order(std::string_view symbol, uint64_t order_id) noexcept;

    [[nodiscard]] std::size_t market_count() const noexcept { return shards_.size(); }
};

} // namespace me
