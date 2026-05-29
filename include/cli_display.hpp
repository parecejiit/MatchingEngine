#pragma once

#include "matching_engine.hpp"
#include <boost/json.hpp>
#include <string>

namespace me {

struct DisplayContext {
    std::string command;
    std::string query_type;
    const HotResult* result{nullptr};
    const boost::json::object* response{nullptr};
    std::size_t ladder_depth{10}; // levels per side in top/ladder view
    bool show_all_levels{false};  // true for query full
};

[[nodiscard]] std::string format_cli_display(MatchingEngine& engine, const DisplayContext& ctx);

} // namespace me
