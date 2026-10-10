#pragma once

#include <vector>

#include "ibs/core/mode/KeyMode.hpp"

namespace ibs::testing {

inline core::KeyMode beat7k() {
    std::vector<core::LaneDefinition> lanes;
    for (int x = 1; x <= 7; ++x) {
        lanes.push_back({x, std::to_string(x), x % 2 == 1 ? core::LaneKind::White : core::LaneKind::Black, 1.0});
    }
    lanes.push_back({8, "SC", core::LaneKind::Scratch, 2.0});
    return core::KeyMode("beat-7k", "7KEYS", lanes, 9);
}

}  // namespace ibs::testing
