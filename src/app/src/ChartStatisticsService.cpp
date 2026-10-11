#include "ibs/app/ChartStatisticsService.hpp"

#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/playback/BmsonRenderer.hpp"

namespace ibs::app {

// Why not 素材ごとの音量を足し合わせる: 同じ素材でも譜面に割り当てたスライスと BGM のスライスがあり、
// 重なり方で音量も変わるため、実際に鳴る bmson を「譜面だけ」「BGM だけ」「全部」で鳴らしてから測る
ChartStatisticsReport computeChartStatistics(const core::Project& project, const core::KeyMode& mode,
                                             const std::map<std::string, const core::AudioBuffer*>& audio, int sampleRate) {
    ChartStatisticsReport report;
    report.notes = core::playableNoteStatistics(project, mode);
    report.bgm = core::bgmStatistics(project, mode);
    report.chartDensity = core::densityOf(report.notes.times);
    report.bgmDensity = core::densityOf(report.bgm.times);
    auto allTimes = report.notes.times;
    allTimes.insert(allTimes.end(), report.bgm.times.begin(), report.bgm.times.end());
    report.totalDensity = core::densityOf(allTimes);

    for (const auto& asset : project.assets()) {
        if (!audio.contains(asset.name)) {
            report.missingAssets.push_back(asset.name);
        }
    }
    const auto document = core::bmson::exportDocument(project);
    const auto playable = [&mode](const core::bmson::Note& note) { return mode.isPlayable(note.x); };
    const auto background = [&mode](const core::bmson::Note& note) { return !mode.isPlayable(note.x); };
    report.chartLevel = core::levelOf(core::renderBmson(document, audio, sampleRate, playable));
    report.bgmLevel = core::levelOf(core::renderBmson(document, audio, sampleRate, background));
    report.totalLevel = core::levelOf(core::renderBmson(document, audio, sampleRate));
    return report;
}

}  // namespace ibs::app
