#include "ibs/core/audio/AudioBuffer.hpp"

namespace ibs::core {

std::vector<float> mixToMono(const AudioBuffer& buffer) {
    const std::size_t frames = buffer.frameCount();
    if (buffer.channels == 1) {
        return std::vector<float>(buffer.interleavedSamples.begin(),
                                  buffer.interleavedSamples.begin() + static_cast<std::ptrdiff_t>(frames));
    }
    std::vector<float> mono(frames, 0.0f);
    const auto channels = static_cast<std::size_t>(buffer.channels);
    const float scale = 1.0f / static_cast<float>(buffer.channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float sum = 0.0f;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            sum += buffer.interleavedSamples[frame * channels + channel];
        }
        mono[frame] = sum * scale;
    }
    return mono;
}

}  // namespace ibs::core
