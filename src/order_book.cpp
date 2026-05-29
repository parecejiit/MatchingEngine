#include "order_book.hpp"

namespace me {

void OrderBook::unlink_from_level(uint32_t slot_idx) noexcept {
    OrderSlot* s = order_pool_.slot(slot_idx);
    if (!s || !s->in_book) return;

    PriceLevel* lvl = level_pool_.get(s->level_idx);
    if (!lvl) return;

    const uint32_t prev = s->prev;
    const uint32_t next = s->next;
    if (prev != INVALID_INDEX) {
        order_pool_.slot(prev)->next = next;
    } else {
        lvl->head = next;
    }
    if (next != INVALID_INDEX) {
        order_pool_.slot(next)->prev = prev;
    } else {
        lvl->tail = prev;
    }
    if (lvl->count > 0) --lvl->count;
    s->prev = s->next = INVALID_INDEX;
    s->in_book = false;
}

void OrderBook::append_to_level(uint32_t level_idx, uint32_t slot_idx) noexcept {
    OrderSlot* s = order_pool_.slot(slot_idx);
    PriceLevel* lvl = level_pool_.get(level_idx);
    if (!s || !lvl) return;

    s->level_idx = level_idx;
    s->prev = lvl->tail;
    s->next = INVALID_INDEX;
    if (lvl->tail != INVALID_INDEX) {
        order_pool_.slot(lvl->tail)->next = slot_idx;
    } else {
        lvl->head = slot_idx;
    }
    lvl->tail = slot_idx;
    ++lvl->count;
    s->in_book = true;
}

void OrderBook::insert_level_sorted(uint32_t level_idx) noexcept {
    PriceLevel* lvl = level_pool_.get(level_idx);
    if (!lvl) return;

    uint32_t& best = (lvl->side == Side::Buy) ? best_bid_level_ : best_ask_level_;

    if (best == INVALID_INDEX) {
        best = level_idx;
        return;
    }

    // Walk price chain: bids = descending (best=highest at best), asks = ascending (best=lowest).
    uint32_t cur = best;
    while (cur != INVALID_INDEX) {
        PriceLevel* c = level_pool_.get(cur);
        if (!c) break;

        const bool insert_before = (lvl->side == Side::Buy)
            ? (lvl->price_ticks > c->price_ticks)
            : (lvl->price_ticks < c->price_ticks);

        if (insert_before) {
            const uint32_t old_prev = c->price_prev;
            c->price_prev = level_idx;
            lvl->price_next = cur;
            lvl->price_prev = old_prev;
            if (old_prev != INVALID_INDEX) {
                level_pool_.get(old_prev)->price_next = level_idx;
            } else {
                best = level_idx;
            }
            return;
        }

        if (c->price_next == INVALID_INDEX) {
            c->price_next = level_idx;
            lvl->price_prev = cur;
            return;
        }
        cur = c->price_next;
    }
}

uint32_t OrderBook::get_or_create_level(Side side, int64_t price_ticks) noexcept {
    TickLevelMap& map = (side == Side::Buy) ? bid_ticks_ : ask_ticks_;
    uint32_t existing = map.find(price_ticks);
    if (existing != INVALID_INDEX) return existing;

    const uint32_t idx = level_pool_.allocate(side, price_ticks);
    if (idx == INVALID_INDEX) return INVALID_INDEX;

    map.insert(price_ticks, idx);
    insert_level_sorted(idx);
    return idx;
}

void OrderBook::remove_level_if_empty(uint32_t level_idx) noexcept {
    PriceLevel* lvl = level_pool_.get(level_idx);
    if (!lvl || lvl->count != 0) return;

    TickLevelMap& map = (lvl->side == Side::Buy) ? bid_ticks_ : ask_ticks_;
    map.erase(lvl->price_ticks);

    uint32_t& best = (lvl->side == Side::Buy) ? best_bid_level_ : best_ask_level_;
    if (best == level_idx) {
        best = lvl->price_next;
    }
    if (lvl->price_prev != INVALID_INDEX) {
        level_pool_.get(lvl->price_prev)->price_next = lvl->price_next;
    }
    if (lvl->price_next != INVALID_INDEX) {
        level_pool_.get(lvl->price_next)->price_prev = lvl->price_prev;
    }

    level_pool_.free(level_idx);
}

void OrderBook::rest_order(uint32_t slot_idx) noexcept {
    OrderSlot* s = order_pool_.slot(slot_idx);
    if (!s) return;

    const int64_t px = s->order.price_ticks;
    const uint32_t lvl_idx = get_or_create_level(s->order.side, px);
    if (lvl_idx == INVALID_INDEX) return;

    append_to_level(lvl_idx, slot_idx);
    order_id_map_.put(s->order.order_id, slot_idx);
}

bool OrderBook::cancel_order(uint64_t order_id) noexcept {
    const uint32_t slot_idx = order_id_map_.find(order_id, [&](uint32_t idx) {
        OrderSlot* s = order_pool_.slot(idx);
        return s && s->order.order_id == order_id;
    });
    if (slot_idx == INVALID_INDEX) return false;

    OrderSlot* s = order_pool_.slot(slot_idx);
    if (!s || s->order.order_id != order_id) return false;

    const uint32_t lvl_idx = s->level_idx;
    unlink_from_level(slot_idx);
    order_id_map_.erase(order_id, slot_idx);
    order_pool_.free(slot_idx);
    remove_level_if_empty(lvl_idx);
    return true;
}

} // namespace me
