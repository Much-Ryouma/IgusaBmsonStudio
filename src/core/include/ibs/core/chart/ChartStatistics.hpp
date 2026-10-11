#pragma once

#include <cstddef>
#include <map>
#include <vector>

#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/mode/KeyMode.hpp"
#include "ibs/core/model/Project.hpp"

namespace ibs::core {

struct DensityStatistics {
    std::size_t events = 0;
    double firstSeconds = 0.0;
    double lastSeconds = 0.0;
    std::size_t maxPerSecond = 0;
    double maxAtSeconds = 0.0;
    double averagePerSecond = 0.0;
};

struct PlayableNoteStatistics {
    std::size_t total = 0;
    std::size_t normal = 0;
    std::size_t longNotes = 0;
    std::size_t scratch = 0;
    std::size_t scratchLong = 0;
    std::map<int, std::size_t> perLane;
    std::vector<double> times;
};

struct BgmStatistics {
    std::size_t slices = 0;
    std::size_t bgmColumnNotes = 0;
    std::vector<double> times;
};

struct LevelStatistics {
    double durationSeconds = 0.0;
    double averageDb = -120.0;
    double peakDb = -120.0;
    double peakAtSeconds = 0.0;
    double loudestSecondDb = -120.0;
    double loudestSecondAtSeconds = 0.0;
};

[[nodiscard]] PlayableNoteStatistics playableNoteStatistics(const Project& project, const KeyMode& mode);
[[nodiscard]] BgmStatistics bgmStatistics(const Project& project, const KeyMode& mode);
[[nodiscard]] DensityStatistics densityOf(std::vector<double> times, double windowSeconds = 1.0);
[[nodiscard]] LevelStatistics levelOf(const AudioBuffer& audio);

}  // namespace ibs::core
