#include "ibs/app/NullTestService.hpp"

#include <algorithm>
#include <exception>

#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/chart/ChartEditing.hpp"
#include "ibs/core/playback/PlaybackRange.hpp"
#include "ibs/core/timing/TimingEdits.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/process/ProcessRunner.hpp"

namespace ibs::app {

int LocalProcessRunner::run(const LaunchRequest& request) {
    return infra::runProcessAndWait(request.program, request.arguments, request.workingDirectory);
}

NullTestService::NullTestService(const ChartService& charts, ChartStorage& storage, AudioSource& audio)
    : charts_(charts), storage_(storage), audio_(audio) {}

// Why not 正解として作者のミックス音源を使う: 手元にあるのはフレーズ素材だけで、bmson の解釈の違いを確かめたいので、
// 「配置どおりに素材をそのまま重ねた音」を正解にする（SPEC §11.4）
core::AudioBuffer NullTestService::referenceMix(const core::Project& project, const ProjectAudio& assets, int sampleRate) const {
    const auto timing = project.timing.toTimingMap();
    std::vector<core::MixSource> sources;
    double end = 0.0;
    for (const auto& placement : project.placements()) {
        const auto found = assets.buffers.find(project.asset(placement.assetId).name);
        if (found == assets.buffers.end()) {
            continue;
        }
        sources.push_back(core::MixSource{placement.id, &found->second, 1.0f});
        end = std::max(end, timing.secondsAt(placement.startPulse) + found->second.durationSeconds());
    }
    return core::renderMix(project, sources, 0.0, end, sampleRate);
}

NullTestReport NullTestService::summarize(const core::Project& project, const core::AudioBuffer& reference,
                                          const core::AudioBuffer& candidate, const ProjectAudio& assets,
                                          const NullTestOptions& options) const {
    NullTestReport report;
    report.missingAssets = assets.missing;
    if (assets.buffers.empty()) {
        report.error = "比較に使える音声素材が見つかりません";
        return report;
    }
    const auto comparison = core::compareRenders(reference, candidate, options.comparison);
    report.completed = true;
    report.offsetMs = comparison.offsetSeconds * 1000.0;
    report.gain = comparison.gain;
    report.residualDb = comparison.residualDb;
    report.passed = comparison.residualDb <= options.thresholdDb;
    auto windows = comparison.windows;
    std::stable_sort(windows.begin(), windows.end(), [](const auto& a, const auto& b) { return a.residualDb > b.residualDb; });
    const auto timing = project.timing.toTimingMap();
    for (const auto& window : windows) {
        if (report.worst.size() >= options.worstCount || window.residualDb <= options.thresholdDb) {
            break;
        }
        const auto pulse = static_cast<core::Pulse>(timing.pulseAt(window.startSeconds));
        report.worst.push_back(NullTestSection{window.startSeconds, window.residualDb, core::barIndexAt(project.timing, pulse), pulse});
    }
    return report;
}

NullTestReport NullTestService::compareWithInternalPlayer(const core::Project& project,
                                                          const std::filesystem::path& chartDirectory,
                                                          const NullTestOptions& options) const {
    try {
        const auto assets = loadProjectAudio(project, chartDirectory, storage_, audio_);
        const auto candidate =
            core::renderBmson(core::bmson::exportDocument(project), assets.byName(), options.internalSampleRate);
        return summarize(project, referenceMix(project, assets, options.internalSampleRate), candidate, assets, options);
    } catch (const std::exception& error) {
        NullTestReport report;
        report.error = error.what();
        return report;
    }
}

NullTestReport NullTestService::compareWithRendering(const core::Project& project, const std::filesystem::path& chartDirectory,
                                                     const core::AudioBuffer& rendered, const NullTestOptions& options) const {
    try {
        const auto assets = loadProjectAudio(project, chartDirectory, storage_, audio_);
        return summarize(project, referenceMix(project, assets, rendered.sampleRate), rendered, assets, options);
    } catch (const std::exception& error) {
        NullTestReport report;
        report.error = error.what();
        return report;
    }
}

NullTestReport NullTestService::compareWithPlayer(const core::Project& project, const std::filesystem::path& chartPath,
                                                  const PlayerSettings& player, const std::string& renderTemplate,
                                                  ProcessRunner& runner, const NullTestOptions& options) const {
    NullTestReport report;
    try {
        if (player.executable.empty()) {
            report.error = "プレーヤーの実行ファイルが設定されていません";
            return report;
        }
        const auto directory = chartPath.parent_path();
        const auto stem = infra::pathToUtf8(chartPath.stem());
        const auto chartCopy = directory / infra::pathFromUtf8(".~nulltest_" + stem + ".bmson");
        const auto output = directory / infra::pathFromUtf8(".~nulltest_" + stem + ".wav");
        core::Project copy = project;
        if (const auto* mode = charts_.keyModeFor(copy); mode != nullptr && player.foldBgmColumns) {
            core::foldBgmColumns(copy, *mode);
        }
        const auto exported = charts_.exportCopy(copy, chartCopy);
        if (!exported.saved) {
            report.error = exported.error;
            return report;
        }
        PlayerSettings renderSettings = player;
        renderSettings.commandTemplate = renderTemplate;
        auto request = buildLaunchRequest(renderSettings, chartCopy, PreviewPosition{});
        for (auto& argument : request.arguments) {
            const std::string placeholder = "{output}";
            for (auto at = argument.find(placeholder); at != std::string::npos; at = argument.find(placeholder)) {
                argument.replace(at, placeholder.size(), infra::pathToUtf8(output));
            }
        }
        const int exitCode = runner.run(request);
        if (exitCode != 0) {
            report.error = "プレーヤーの書き出しが失敗しました（終了コード " + std::to_string(exitCode) + "）";
            return report;
        }
        return compareWithRendering(project, directory, audio_.load(output), options);
    } catch (const std::exception& error) {
        report.error = error.what();
        return report;
    }
}

}  // namespace ibs::app
