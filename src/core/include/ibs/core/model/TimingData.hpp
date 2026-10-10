#pragma once

#include <optional>
#include <vector>

#include "ibs/core/timing/TimingMap.hpp"

namespace ibs::core {

inline constexpr int kDefaultResolution = 240;

struct TimingData {
    double initialBpm = 120.0;
    int resolution = kDefaultResolution;
    std::vector<BpmEvent> bpmEvents;
    std::vector<StopEvent> stopEvents;
    std::optional<std::vector<Pulse>> barLines;

    [[nodiscard]] TimingMap toTimingMap() const { return TimingMap(initialBpm, resolution, bpmEvents, stopEvents); }

    friend bool operator==(const TimingData&, const TimingData&) = default;
};

}  // namespace ibs::core
