#include "matching_engine.hpp"
#include <limits>
#include <vector>

namespace me {

void MatchingEngine::update_top_of_book(HotResult& out) noexcept {
    out.has_bid = out.has_ask = false;
    const uint32_t bid = book_.best_level(Side::Buy);
    const uint32_t ask = book_.best_level(Side::Sell);
    if (bid != INVALID_INDEX) {
        if (const PriceLevel* l = book_.level(bid)) {
            out.best_bid_ticks = l->price_ticks;
            out.has_bid = true;
        }
    }
    if (ask != INVALID_INDEX) {
        if (const PriceLevel* l = book_.level(ask)) {
            out.best_ask_ticks = l->price_ticks;
            out.has_ask = true;
        }
    }
}

HotResult MatchingEngine::place_order(PlaceRequest&& req) noexcept {
    HotResult out{};
    ++timestamp_;

    OrderHandle h = pool_.allocate();
    if (!h.valid()) return out;

    const uint32_t taker_idx = h.index();
    Order& taker = h.order();
    taker.order_id = next_order_id_++;
    taker.account_id = req.account_id;
    taker.side = req.side;
    taker.size = req.size;
    taker.remaining = req.size;
    taker.tif = req.tif;
    taker.timestamp = timestamp_;
    taker.is_market = !req.limit_ticks.has_value();
    taker.price_ticks = req.limit_ticks.value_or(
        (req.side == Side::Buy) ? std::numeric_limits<int64_t>::max()
                                : std::numeric_limits<int64_t>::min());

    const Side opp = (taker.side == Side::Buy) ? Side::Sell : Side::Buy;

    if (req.post_only && !taker.is_market) {
        const uint32_t opp_lvl = book_.best_level(opp);
        if (opp_lvl != INVALID_INDEX) {
            if (const PriceLevel* ol = book_.level(opp_lvl)) {
                if (price_crosses(taker.side, taker.price_ticks, false, ol->price_ticks)) {
                    pool_.free(taker_idx);
                    out.rejected_post_only = true;
                    out.placed_order_id = 0;
                    update_top_of_book(out);
                    return out;
                }
            }
        }
    }

    while (taker.remaining > 0) {
        const uint32_t lvl_idx = book_.best_level(opp);
        if (lvl_idx == INVALID_INDEX) break;

        PriceLevel* lvl = book_.levels().get(lvl_idx);
        if (!lvl || lvl->head == INVALID_INDEX) break;
        if (!price_crosses(taker.side, taker.price_ticks, taker.is_market, lvl->price_ticks)) break;

        uint32_t cur = lvl->head;
        while (cur != INVALID_INDEX && taker.remaining > 0) {
            OrderSlot* maker_s = book_.slot(cur);
            if (!maker_s) break;
            const uint32_t next_maker = maker_s->next;
            Order& maker = maker_s->order;

            if (maker.account_id == taker.account_id) {
                out.rejected_self_trade = true;
                pool_.free(taker_idx);
                update_top_of_book(out);
                return out;
            }

            const uint64_t fill_sz = std::min(taker.remaining, maker.remaining);
            if (out.fill_count < MAX_FILLS_PER_OP) {
                Fill& f = out.fills[out.fill_count++];
                f.maker_order_id = maker.order_id;
                f.taker_order_id = taker.order_id;
                f.size = fill_sz;
                f.price_ticks = lvl->price_ticks;
                f.timestamp = timestamp_;
                on_fill(f);
            }

            maker.remaining -= fill_sz;
            taker.remaining -= fill_sz;

            if (maker.remaining == 0) {
                const uint64_t oid = maker.order_id;
                book_.id_map().erase(oid, cur);
                book_.unlink_from_level(cur);
                pool_.free(cur);
                if (PriceLevel* l2 = book_.levels().get(lvl_idx); l2 && l2->count == 0) {
                    book_.remove_level_if_empty(lvl_idx);
                }
            }

            cur = next_maker;
            lvl = book_.levels().get(lvl_idx);
            if (!lvl) break;
        }
    }

    out.placed_order_id = taker.order_id;

    if (taker.remaining > 0 && taker.tif == TimeInForce::GTC && !taker.is_market) {
        book_.rest_order(taker_idx);
        out.resting_order_id = taker.order_id;
        out.accepted = true;
    } else {
        OrderSlot* ts = book_.slot(taker_idx);
        if (ts && !ts->in_book) pool_.free(taker_idx);
        out.accepted = true;
    }

    update_top_of_book(out);
    return out;
}

HotResult MatchingEngine::cancel_order(uint64_t order_id) noexcept {
    HotResult out{};
    out.accepted = book_.cancel_order(order_id);
    update_top_of_book(out);
    return out;
}

HotResult MatchingEngine::query_top() noexcept {
    HotResult out{};
    update_top_of_book(out);
    return out;
}

HotResult MatchingEngine::query_order(uint64_t order_id) noexcept {
    HotResult out{};
    const uint32_t idx = book_.id_map().find(order_id, [&](uint32_t i) {
        OrderSlot* s = book_.slot(i);
        return s && s->order.order_id == order_id;
    });
    if (idx != INVALID_INDEX) {
        if (OrderSlot* s = book_.slot(idx)) {
            out.resting_order_id = order_id;
            out.query_remaining = s->order.remaining;
            out.query_price_ticks = s->order.price_ticks;
            out.accepted = true;
        }
    }
    update_top_of_book(out);
    return out;
}

std::vector<MatchingEngine::DepthLevel> MatchingEngine::depth_snapshot(Side side) {
    std::vector<DepthLevel> out;
    uint32_t cur = book_.best_level(side);
    while (cur != INVALID_INDEX) {
        const PriceLevel* lvl = book_.level(cur);
        if (!lvl) break;
        DepthLevel d{};
        d.price_ticks = lvl->price_ticks;
        uint32_t n = lvl->head;
        while (n != INVALID_INDEX) {
            if (const OrderSlot* s = book_.slot(n)) {
                d.total_size += s->order.remaining;
                ++d.order_count;
                n = s->next;
            } else break;
        }
        out.push_back(d);
        cur = lvl->price_next;
    }
    return out;
}

} // namespace me
