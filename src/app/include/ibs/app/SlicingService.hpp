#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ibs/app/ChartStorage.hpp"
#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/slicing/CutPlanner.hpp"
#include "ibs/core/slicing/OnsetDetector.hpp"

namespace ibs::app {

class AudioSource {
public:
    virtual ~AudioSource() = default;

    [[nodiscard]] virtual core::AudioBuffer load(const std::filesystem::path& path) = 0;
};

class LocalAudioSource final : public AudioSource {
public:
    [[nodiscard]] core::AudioBuffer load(const std::filesystem::path& path) override;
};

struct SliceSettings {
    core::OnsetParameters onset;
    core::QuantizeParameters quantize;
};

struct SliceOutcome {
    bool succeeded = false;
    std::size_t detectedOnsets = 0;
    core::CutPlan plan;
    core::ApplyResult applied;
    std::vector<core::Issue> issues;
    std::string error;
};

class SlicingService {
public:
    SlicingService(ChartStorage& storage, AudioSource& audio);

    [[nodiscard]] SliceOutcome slicePlacement(core::Project& project, core::PlacementId placementId,
                                              const std::filesystem::path& chartDirectory,
                                              const SliceSettings& settings) const;

    [[nodiscard]] static SliceOutcome sliceBuffer(core::Project& project, core::PlacementId placementId,
                                                  const core::AudioBuffer& audio, const SliceSettings& settings);

private:
    ChartStorage& storage_;
    AudioSource& audio_;
};

}  // namespace ibs::app
