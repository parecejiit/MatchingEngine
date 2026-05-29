#pragma once

#include "types.hpp"
#include <array>
#include <cstdint>

namespace me {

// Intrusive queue node in the order pool (no heap).
struct OrderSlot {
    Order order{};
    uint32_t prev{INVALID_INDEX};
    uint32_t next{INVALID_INDEX};
    uint32_t level_idx{INVALID_INDEX};
    bool in_book{false};
};

struct PriceLevel {
    int64_t price_ticks{0};
    Side side{Side::Buy};
    uint32_t head{INVALID_INDEX};
    uint32_t tail{INVALID_INDEX};
    uint32_t count{0};
    uint32_t price_prev{INVALID_INDEX}; // worse price (lower bid / higher ask)
    uint32_t price_next{INVALID_INDEX}; // better price toward matching
    bool active{false};
};

// Move-only handle; zero copy of Order on hot path after construction in-place.
class OrderHandle {
    OrderSlot* slot_{nullptr};
    uint32_t idx_{INVALID_INDEX};

public:
    OrderHandle() = default;
    OrderHandle(OrderSlot* slot, uint32_t idx) noexcept : slot_(slot), idx_(idx) {}

    OrderHandle(const OrderHandle&) = delete;
    OrderHandle& operator=(const OrderHandle&) = delete;

    OrderHandle(OrderHandle&& o) noexcept : slot_(o.slot_), idx_(o.idx_) {
        o.slot_ = nullptr;
        o.idx_ = INVALID_INDEX;
    }
    OrderHandle& operator=(OrderHandle&& o) noexcept {
        if (this != &o) {
            slot_ = o.slot_;
            idx_ = o.idx_;
            o.slot_ = nullptr;
            o.idx_ = INVALID_INDEX;
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept { return slot_ != nullptr; }
    [[nodiscard]] uint32_t index() const noexcept { return idx_; }
    [[nodiscard]] Order& order() noexcept { return slot_->order; }
    [[nodiscard]] const Order& order() const noexcept { return slot_->order; }
    [[nodiscard]] OrderSlot& slot() noexcept { return *slot_; }
};

class OrderPool {
    std::array<OrderSlot, MAX_ORDERS> slots_{};
    std::array<bool, MAX_ORDERS> used_{};
    uint32_t free_head_{INVALID_INDEX};
    uint32_t free_tail_{INVALID_INDEX};

    void push_free(uint32_t idx) noexcept {
        slots_[idx].next = INVALID_INDEX;
        if (free_tail_ == INVALID_INDEX) {
            free_head_ = free_tail_ = idx;
        } else {
            slots_[free_tail_].next = idx;
            free_tail_ = idx;
        }
    }

public:
    OrderPool() {
        for (uint32_t i = 0; i < MAX_ORDERS; ++i) {
            push_free(i);
        }
    }

    [[nodiscard]] OrderHandle allocate() noexcept {
        if (free_head_ == INVALID_INDEX) return {};
        const uint32_t idx = free_head_;
        free_head_ = slots_[idx].next;
        if (free_head_ == INVALID_INDEX) free_tail_ = INVALID_INDEX;
        used_[idx] = true;
        slots_[idx] = OrderSlot{};
        slots_[idx].in_book = false;
        return OrderHandle{&slots_[idx], idx};
    }

    void free(uint32_t idx) noexcept {
        if (idx >= MAX_ORDERS || !used_[idx]) return;
        used_[idx] = false;
        slots_[idx] = OrderSlot{};
        push_free(idx);
    }

    [[nodiscard]] OrderSlot* slot(uint32_t idx) noexcept {
        return (idx < MAX_ORDERS && used_[idx]) ? &slots_[idx] : nullptr;
    }
};

class LevelPool {
    std::array<PriceLevel, MAX_LEVELS> levels_{};
    std::array<bool, MAX_LEVELS> used_{};
    uint32_t free_head_{INVALID_INDEX};

    void push_free(uint32_t idx) noexcept {
        levels_[idx].price_next = free_head_;
        free_head_ = idx;
    }

public:
    LevelPool() {
        for (uint32_t i = 0; i < MAX_LEVELS; ++i) push_free(i);
    }

    [[nodiscard]] uint32_t allocate(Side side, int64_t price_ticks) noexcept {
        if (free_head_ == INVALID_INDEX) return INVALID_INDEX;
        const uint32_t idx = free_head_;
        free_head_ = levels_[idx].price_next;
        used_[idx] = true;
        levels_[idx] = PriceLevel{};
        levels_[idx].side = side;
        levels_[idx].price_ticks = price_ticks;
        levels_[idx].active = true;
        return idx;
    }

    void free(uint32_t idx) noexcept {
        if (idx >= MAX_LEVELS || !used_[idx]) return;
        used_[idx] = false;
        levels_[idx] = PriceLevel{};
        push_free(idx);
    }

    [[nodiscard]] PriceLevel* get(uint32_t idx) noexcept {
        return (idx < MAX_LEVELS && used_[idx]) ? &levels_[idx] : nullptr;
    }
};

// Open-addressing map: order_id % MAX_ORDER_ID_MAP -> slot index (linear probe).
class OrderIdMap {
    std::array<uint32_t, MAX_ORDER_ID_MAP> table_{};
    static constexpr uint32_t TOMBSTONE = MAX_ORDERS; // sentinel > any valid index

public:
    OrderIdMap() { table_.fill(INVALID_INDEX); }

    void put(uint64_t order_id, uint32_t slot_idx) noexcept {
        uint64_t h = order_id % MAX_ORDER_ID_MAP;
        for (std::size_t i = 0; i < MAX_ORDER_ID_MAP; ++i) {
            const std::size_t pos = (h + i) % MAX_ORDER_ID_MAP;
            if (table_[pos] == INVALID_INDEX || table_[pos] == TOMBSTONE) {
                table_[pos] = slot_idx;
                return;
            }
        }
    }

    template <class F>
    [[nodiscard]] uint32_t find(uint64_t order_id, F&& slot_matches) const noexcept {
        uint64_t h = order_id % MAX_ORDER_ID_MAP;
        for (std::size_t i = 0; i < MAX_ORDER_ID_MAP; ++i) {
            const std::size_t pos = (h + i) % MAX_ORDER_ID_MAP;
            if (table_[pos] == INVALID_INDEX) return INVALID_INDEX;
            if (table_[pos] != TOMBSTONE && slot_matches(table_[pos])) return table_[pos];
        }
        return INVALID_INDEX;
    }

    void erase(uint64_t order_id, uint32_t slot_idx) noexcept {
        uint64_t h = order_id % MAX_ORDER_ID_MAP;
        for (std::size_t i = 0; i < MAX_ORDER_ID_MAP; ++i) {
            const std::size_t pos = (h + i) % MAX_ORDER_ID_MAP;
            if (table_[pos] == slot_idx) {
                table_[pos] = TOMBSTONE;
                return;
            }
            if (table_[pos] == INVALID_INDEX) return;
        }
    }
};

// price_ticks -> level index per side (linear probe on ticks hash).
class TickLevelMap {
    static constexpr std::size_t CAP = 8192;
    struct Entry {
        int64_t tick{0};
        uint32_t level_idx{INVALID_INDEX};
        bool used{false};
    };
    std::array<Entry, CAP> table_{};

    [[nodiscard]] std::size_t probe(int64_t tick) const noexcept {
        const uint64_t h = static_cast<uint64_t>(tick);
        return static_cast<std::size_t>(h % CAP);
    }

public:
    [[nodiscard]] uint32_t find(int64_t tick) const noexcept {
        std::size_t pos = probe(tick);
        for (std::size_t i = 0; i < CAP; ++i) {
            const std::size_t p = (pos + i) % CAP;
            if (!table_[p].used) return INVALID_INDEX;
            if (table_[p].tick == tick) return table_[p].level_idx;
        }
        return INVALID_INDEX;
    }

    void insert(int64_t tick, uint32_t level_idx) noexcept {
        std::size_t pos = probe(tick);
        for (std::size_t i = 0; i < CAP; ++i) {
            const std::size_t p = (pos + i) % CAP;
            if (!table_[p].used) {
                table_[p] = Entry{tick, level_idx, true};
                return;
            }
        }
    }

    void erase(int64_t tick) noexcept {
        std::size_t pos = probe(tick);
        for (std::size_t i = 0; i < CAP; ++i) {
            const std::size_t p = (pos + i) % CAP;
            if (!table_[p].used) return;
            if (table_[p].tick == tick) {
                table_[p].used = false;
                return;
            }
        }
    }
};

} // namespace me
