#include "market_router.hpp"

namespace me {

MarketShard& MarketRouter::shard(std::string_view symbol) {
    const std::string key(symbol);
    auto it = shards_.find(key);
    if (it == shards_.end()) {
        auto ins = shards_.emplace(key, std::make_unique<MarketShard>(key));
        return *ins.first->second;
    }
    return *it->second;
}

RoutedResult MarketRouter::place_order(std::string_view symbol, PlaceRequest&& req) noexcept {
    MarketShard& s = shard(symbol);
    RoutedResult rr{};
    rr.shard = &s;
    rr.named_market = !symbol.empty();
    rr.hot = s.place_order(std::move(req));
    return rr;
}

RoutedResult MarketRouter::cancel_order(std::string_view symbol, uint64_t order_id) noexcept {
    MarketShard& s = shard(symbol);
    RoutedResult rr{};
    rr.shard = &s;
    rr.named_market = !symbol.empty();
    rr.hot = s.cancel_order(order_id);
    return rr;
}

RoutedResult MarketRouter::query_top(std::string_view symbol) noexcept {
    MarketShard& s = shard(symbol);
    RoutedResult rr{};
    rr.shard = &s;
    rr.named_market = !symbol.empty();
    rr.hot = s.query_top();
    return rr;
}

RoutedResult MarketRouter::query_order(std::string_view symbol, uint64_t order_id) noexcept {
    MarketShard& s = shard(symbol);
    RoutedResult rr{};
    rr.shard = &s;
    rr.named_market = !symbol.empty();
    rr.hot = s.query_order(order_id);
    return rr;
}

} // namespace me
