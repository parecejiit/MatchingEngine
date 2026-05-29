#pragma once

#include "memory_pool.hpp"

namespace me {

class MatchingEngine;

// Intrusive price-time FIFO book; O(1) cancel; cached best levels.
class OrderBook {
    friend class MatchingEngine;
    OrderPool& order_pool_;
    LevelPool level_pool_;
    OrderIdMap order_id_map_;
    TickLevelMap bid_ticks_;
    TickLevelMap ask_ticks_;

    uint32_t best_bid_level_{INVALID_INDEX};
    uint32_t best_ask_level_{INVALID_INDEX};

    void unlink_from_level(uint32_t slot_idx) noexcept;
    void append_to_level(uint32_t level_idx, uint32_t slot_idx) noexcept;
    [[nodiscard]] uint32_t get_or_create_level(Side side, int64_t price_ticks) noexcept;
    void remove_level_if_empty(uint32_t level_idx) noexcept;
    void insert_level_sorted(uint32_t level_idx) noexcept;

public:
    explicit OrderBook(OrderPool& pool) noexcept : order_pool_(pool) {}

    void rest_order(uint32_t slot_idx) noexcept;
    [[nodiscard]] bool cancel_order(uint64_t order_id) noexcept;

    [[nodiscard]] uint32_t best_level(Side side) const noexcept {
        return side == Side::Buy ? best_bid_level_ : best_ask_level_;
    }

    [[nodiscard]] LevelPool& levels() noexcept { return level_pool_; }
    [[nodiscard]] OrderPool& orders() noexcept { return order_pool_; }
    [[nodiscard]] OrderIdMap& id_map() noexcept { return order_id_map_; }

    [[nodiscard]] PriceLevel* level(uint32_t idx) noexcept {
        return level_pool_.get(idx);
    }

    [[nodiscard]] OrderSlot* slot(uint32_t idx) noexcept { return order_pool_.slot(idx); }
};

} // namespace me
