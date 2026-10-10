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

[[nodiscard]] std::optional<TimeSignature> timeSignatureForBarLength(Pulse barLengthPulses, int resolution);

}  // namespace ibs::core
