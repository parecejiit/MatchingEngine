#pragma once

#include "matching_engine.hpp"
#include <boost/json.hpp>
#include <string>

namespace me {

struct IoLineResult {
    std::string json;
    std::string display; // tabulated summary (empty for comments / when disabled)
};

// Cold path only: JSON <-> engine. No copies into hot structures beyond parse boundary.
class IoAdapter {
    MatchingEngine& engine_;

public:
    explicit IoAdapter(MatchingEngine& engine) noexcept : engine_(engine) {}

    [[nodiscard]] IoLineResult handle_line(std::string line, bool pretty = false);
};

} // namespace me
