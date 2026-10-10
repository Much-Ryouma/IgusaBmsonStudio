#pragma once

#include <vector>

#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/model/Project.hpp"

namespace ibs::core {

struct SampleRange {
    SampleIndex begin = 0;
    SampleIndex end = 0;

    [[nodiscard]] bool empty() const noexcept { return end <= begin; }
    friend bool operator==(const SampleRange&, const SampleRange&) = default;
};

[[nodiscard]] SampleIndex sampleOffsetInPlacement(const Project& project, PlacementId placementId, Pulse songPulse,
                                                  int sampleRate);

[[nodiscard]] SampleRange sliceRange(const Project& project, CutPointId cutPointId, int sampleRate,
                                     SampleIndex totalFrames);

[[nodiscard]] Pulse placementEndPulse(const Project& project, PlacementId placementId, double durationSeconds);

struct MixSource {
    PlacementId placementId;
    const AudioBuffer* audio = nullptr;
    float gain = 1.0f;
};

[[nodiscard]] AudioBuffer renderRange(const AudioBuffer& audio, SampleRange range, int outputSampleRate);

[[nodiscard]] AudioBuffer renderMix(const Project& project, const std::vector<MixSource>& sources,
                                    double fromSongSeconds, double toSongSeconds, int outputSampleRate);

}  // namespace ibs::core
