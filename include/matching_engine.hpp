#pragma once

#include "order_book.hpp"
#include <boost/signals2/signal.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace me {

struct Fill {
    uint64_t maker_order_id{0};
    uint64_t taker_order_id{0};
    uint64_t size{0};
    int64_t price_ticks{0};
    uint64_t timestamp{0};
};

struct PlaceRequest {
    uint64_t account_id{0};
    Side side{Side::Buy};
    std::optional<int64_t> limit_ticks; // nullopt => market
    uint64_t size{0};
    TimeInForce tif{TimeInForce::GTC};
    bool post_only{false};
};

struct HotResult {
    std::array<Fill, MAX_FILLS_PER_OP> fills{};
    std::size_t fill_count{0};
    bool accepted{false};
    bool rejected_self_trade{false};
    bool rejected_post_only{false};
    uint64_t placed_order_id{0};
    std::optional<uint64_t> resting_order_id;
    int64_t best_bid_ticks{0};
    int64_t best_ask_ticks{0};
    bool has_bid{false};
    bool has_ask{false};
    std::optional<uint64_t> query_remaining;
    std::optional<int64_t> query_price_ticks;
};

class MatchingEngine {
    OrderPool pool_;
    OrderBook book_;
    uint64_t next_order_id_{1};
    uint64_t timestamp_{0};

    void update_top_of_book(HotResult& out) noexcept;

public:
    MatchingEngine() noexcept : book_(pool_) {}

    boost::signals2::signal<void(const Fill&)> on_fill;

    [[nodiscard]] HotResult place_order(PlaceRequest&& req) noexcept;
    [[nodiscard]] HotResult cancel_order(uint64_t order_id) noexcept;
    [[nodiscard]] HotResult query_top() noexcept;
    [[nodiscard]] HotResult query_order(uint64_t order_id) noexcept;

    // Cold-path helpers for depth snapshot (allocates — not for hot path).
    struct DepthLevel {
        int64_t price_ticks{0};
        uint64_t total_size{0};
        std::size_t order_count{0};
        std::size_t level_rank{0}; // 1 = best price on side
    };
    [[nodiscard]] std::vector<DepthLevel> depth_snapshot(Side side);

    // Cold path: resting orders for CLI display (price-time order per level).
    struct BookOrderRow {
        uint64_t order_id{0};
        uint64_t account_id{0};
        Side side{Side::Buy};
        int64_t price_ticks{0};
        uint64_t remaining{0};
        uint64_t timestamp{0};
        std::size_t queue_position{0};
        std::size_t price_level{0}; // 1 = best bid / best ask
    };
    [[nodiscard]] std::vector<BookOrderRow> book_orders();
};

} // namespace me
