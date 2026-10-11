#pragma once

#include <map>
#include <string>
#include <vector>

#include "ibs/core/chart/ChartStatistics.hpp"

namespace ibs::app {

struct ChartStatisticsReport {
    core::PlayableNoteStatistics notes;
    core::BgmStatistics bgm;
    core::DensityStatistics chartDensity;
    core::DensityStatistics bgmDensity;
    core::DensityStatistics totalDensity;
    core::LevelStatistics chartLevel;
    core::LevelStatistics bgmLevel;
    core::LevelStatistics totalLevel;
    std::vector<std::string> missingAssets;
};

[[nodiscard]] ChartStatisticsReport computeChartStatistics(const core::Project& project, const core::KeyMode& mode,
                                                           const std::map<std::string, const core::AudioBuffer*>& audio,
                                                           int sampleRate = 44100);

}  // namespace ibs::app
