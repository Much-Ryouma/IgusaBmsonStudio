#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ibs::core {

using SampleIndex = std::int64_t;

struct AudioBuffer {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> interleavedSamples;

    [[nodiscard]] std::size_t frameCount() const noexcept {
        return channels > 0 ? interleavedSamples.size() / static_cast<std::size_t>(channels) : 0;
    }
    [[nodiscard]] double durationSeconds() const noexcept {
        return sampleRate > 0 ? static_cast<double>(frameCount()) / sampleRate : 0.0;
    }
};

[[nodiscard]] std::vector<float> mixToMono(const AudioBuffer& buffer);

}  // namespace ibs::core
