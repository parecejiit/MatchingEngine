#include "market_shard.hpp"

namespace me {

HotResult MarketShard::place_order(PlaceRequest&& req) noexcept {
    HotResult hot = engine_.place_order(std::move(req));
    const bool ok = hot.accepted && !hot.rejected_self_trade && !hot.rejected_post_only;
    MarketEvent& ev = events_.emplace_back();
    ev.seq = next_event_seq_++;
    ev.kind = MarketEventKind::Place;
    ev.ok = ok;
    ev.order_id = hot.placed_order_id != 0 ? hot.placed_order_id
                 : hot.resting_order_id ? *hot.resting_order_id
                                        : 0;

    for (std::size_t i = 0; i < hot.fill_count; ++i) {
        const Fill& f = hot.fills[i];
        events_.push_back(MarketEvent{
            .seq = next_event_seq_++,
            .kind = MarketEventKind::Fill,
            .ok = true,
            .maker_order_id = f.maker_order_id,
            .taker_order_id = f.taker_order_id,
            .size = f.size,
            .price_ticks = f.price_ticks,
        });
    }
    return hot;
}

HotResult MarketShard::cancel_order(uint64_t order_id) noexcept {
    HotResult hot = engine_.cancel_order(order_id);
    MarketEvent& ev = events_.emplace_back();
    ev.seq = next_event_seq_++;
    ev.kind = MarketEventKind::Cancel;
    ev.ok = hot.accepted;
    ev.order_id = order_id;
    return hot;
}

HotResult MarketShard::query_top() noexcept {
    HotResult hot = engine_.query_top();
    MarketEvent& ev = events_.emplace_back();
    ev.seq = next_event_seq_++;
    ev.kind = MarketEventKind::Query;
    ev.ok = true;
    return hot;
}

HotResult MarketShard::query_order(uint64_t order_id) noexcept {
    HotResult hot = engine_.query_order(order_id);
    MarketEvent& ev = events_.emplace_back();
    ev.seq = next_event_seq_++;
    ev.kind = MarketEventKind::Query;
    ev.ok = hot.accepted;
    ev.order_id = order_id;
    return hot;
}

std::vector<MarketEvent> MarketShard::events_since(uint64_t from_seq, std::size_t limit) const {
    std::vector<MarketEvent> out;
    out.reserve(limit);
    for (const MarketEvent& e : events_) {
        if (e.seq < from_seq) continue;
        out.push_back(e);
        if (out.size() >= limit) break;
    }
    return out;
}

} // namespace me
