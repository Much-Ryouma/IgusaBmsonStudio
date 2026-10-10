#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ibs/core/timing/TimingMap.hpp"

namespace ibs::core {

struct BgaHeader {
    std::uint32_t id = 0;
    std::string name;

    friend bool operator==(const BgaHeader&, const BgaHeader&) = default;
};

struct BgaEvent {
    Pulse y = 0;
    std::uint32_t id = 0;

    friend bool operator==(const BgaEvent&, const BgaEvent&) = default;
};

struct Bga {
    std::vector<BgaHeader> headers;
    std::vector<BgaEvent> bgaEvents;
    std::vector<BgaEvent> layerEvents;
    std::vector<BgaEvent> poorEvents;

    friend bool operator==(const Bga&, const Bga&) = default;
};

}  // namespace ibs::core
