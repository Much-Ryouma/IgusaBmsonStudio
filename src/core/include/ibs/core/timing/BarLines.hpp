#pragma once

#include <optional>
#include <vector>

#include "ibs/core/model/TimingData.hpp"

namespace ibs::core {

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;

    friend bool operator==(const TimeSignature&, const TimeSignature&) = default;
};

[[nodiscard]] std::vector<Pulse> barStartsOf(const TimingData& timing);

enum class GridLineKind { Bar, Beat, Subdivision };

struct GridLine {
    Pulse pulse = 0;
    GridLineKind kind = GridLineKind::Subdivision;
    int barIndex = 0;

    friend bool operator==(const GridLine&, const GridLine&) = default;
};

[[nodiscard]] std::vector<GridLine> gridLinesBetween(const TimingData& timing, int divisionsPerWholeNote, Pulse from,
                                                     Pulse to);

[[nodiscard]] std::optional<TimeSignature> timeSignatureForBarLength(Pulse barLengthPulses, int resolution);

}  // namespace ibs::core
