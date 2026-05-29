#pragma once

#include "market_router.hpp"
#include <boost/json.hpp>
#include <string>

namespace me {

struct IoLineResult {
    std::string json;
    std::string display; // tabulated summary (empty for comments / when disabled)
};

// Cold path only: JSON <-> router -> per-market engine. No copies on hot path beyond parse.
class IoAdapter {
    MarketRouter& router_;

public:
    explicit IoAdapter(MarketRouter& router) noexcept : router_(router) {}

    [[nodiscard]] IoLineResult handle_line(std::string line, bool pretty = false);
};

} // namespace me
