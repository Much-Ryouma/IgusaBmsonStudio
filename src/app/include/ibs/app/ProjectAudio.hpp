#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "ibs/app/ChartService.hpp"
#include "ibs/app/SlicingService.hpp"
#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/model/Project.hpp"

namespace ibs::app {

struct ProjectAudio {
    std::map<std::string, core::AudioBuffer> buffers;
    std::vector<std::string> missing;

    [[nodiscard]] std::map<std::string, const core::AudioBuffer*> byName() const;
};

[[nodiscard]] ProjectAudio loadProjectAudio(const core::Project& project, const std::filesystem::path& chartDirectory,
                                            ChartStorage& storage, AudioSource& audio);

}  // namespace ibs::app
