#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>

namespace me {

enum class MarketEventKind : uint8_t {
    Place,
    Cancel,
    Query,
    Fill,
};

struct MarketEvent {
    uint64_t seq{0};
    MarketEventKind kind{MarketEventKind::Query};
    uint64_t order_id{0};
    uint64_t maker_order_id{0};
    uint64_t taker_order_id{0};
    uint64_t size{0};
    int64_t price_ticks{0};
    bool ok{true};
};

} // namespace me
