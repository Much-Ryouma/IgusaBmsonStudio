#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "ibs/core/audio/AudioBuffer.hpp"

namespace ibs::core {

struct WaveformLevel {
    std::size_t samplesPerBin = 0;
    std::vector<float> minimum;
    std::vector<float> maximum;
};

class WaveformPyramid {
public:
    explicit WaveformPyramid(const std::vector<float>& mono, std::size_t baseSamplesPerBin = 64,
                             std::size_t reductionFactor = 4, std::size_t levelCount = 6);

    [[nodiscard]] std::size_t sampleCount() const noexcept { return sampleCount_; }
    [[nodiscard]] const std::vector<WaveformLevel>& levels() const noexcept { return levels_; }

    [[nodiscard]] std::pair<float, float> range(SampleIndex begin, SampleIndex end) const;

private:
    std::size_t sampleCount_;
    std::vector<WaveformLevel> levels_;
};

}  // namespace ibs::core
