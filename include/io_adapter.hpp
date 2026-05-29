#pragma once

#include "matching_engine.hpp"
#include <boost/json.hpp>
#include <string>

namespace me {

// Cold path only: JSON <-> engine. No copies into hot structures beyond parse boundary.
class IoAdapter {
    MatchingEngine& engine_;

public:
    explicit IoAdapter(MatchingEngine& engine) noexcept : engine_(engine) {}

    [[nodiscard]] std::string handle_line(std::string line);
};

} // namespace me
