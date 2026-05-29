#pragma once

#include <cstdint>
#include <cmath>
#include <limits>
#include <optional>

namespace me {

constexpr std::size_t MAX_ORDERS = 65536;
constexpr std::size_t MAX_LEVELS = 4096;
constexpr std::size_t MAX_FILLS_PER_OP = 256;
constexpr std::size_t MAX_ORDER_ID_MAP = 65536;
constexpr int64_t PRICE_SCALE = 100'000'000; // 1e8 ticks per unit
constexpr uint32_t INVALID_INDEX = UINT32_MAX;

enum class Side : uint8_t { Buy, Sell };
enum class TimeInForce : uint8_t { GTC, IOC };

enum class SelfTradePolicy : uint8_t { CancelNewest };

struct Order {
    uint64_t order_id{0};
    uint64_t account_id{0};
    Side side{Side::Buy};
    int64_t price_ticks{0}; // limit price in ticks; ignored when is_market
    uint64_t size{0};
    uint64_t remaining{0};
    TimeInForce tif{TimeInForce::GTC};
    uint64_t timestamp{0};
    bool is_market{false};
};

[[nodiscard]] inline int64_t price_to_ticks(double price) noexcept {
    return static_cast<int64_t>(std::llround(price * static_cast<double>(PRICE_SCALE)));
}

[[nodiscard]] inline double ticks_to_price(int64_t ticks) noexcept {
    return static_cast<double>(ticks) / static_cast<double>(PRICE_SCALE);
}

[[nodiscard]] inline bool is_market_buy(const Order& o) noexcept {
    return o.is_market && o.side == Side::Buy;
}

[[nodiscard]] inline bool is_market_sell(const Order& o) noexcept {
    return o.is_market && o.side == Side::Sell;
}

[[nodiscard]] inline bool price_crosses(Side taker_side, int64_t limit_ticks, bool taker_is_market,
                                      int64_t level_ticks) noexcept {
    if (taker_is_market) return true;
    if (taker_side == Side::Buy) return limit_ticks >= level_ticks;
    return limit_ticks <= level_ticks;
}

} // namespace me
