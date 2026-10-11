#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "CliSupport.hpp"
#include "ExitCode.hpp"
#include "ibs/app/ChartService.hpp"
#include "ibs/app/ChartStatisticsService.hpp"
#include "ibs/app/ProjectAudio.hpp"
#include "ibs/core/timing/TimingEdits.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::cli {

namespace {

struct StatsArguments {
    std::string chartFile;
    int sampleRate = 44100;
};

struct Positioner {
    const core::TimingData& timing;
    core::TimingMap map = timing.toTimingMap();

    [[nodiscard]] int barAt(double seconds) const {
        return core::barIndexAt(timing, static_cast<core::Pulse>(map.pulseAt(seconds)));
    }
    [[nodiscard]] std::string text(double seconds) const {
        return formatClock(seconds) + " (bar " + std::to_string(barAt(seconds)) + ")";
    }
    [[nodiscard]] nlohmann::ordered_json json(double seconds) const {
        return {{"seconds", seconds}, {"bar", barAt(seconds)}};
    }
};

nlohmann::ordered_json densityJson(const core::DensityStatistics& density, const Positioner& at) {
    return {{"events", density.events},
            {"maxPerSecond", density.maxPerSecond},
            {"maxAt", at.json(density.maxAtSeconds)},
            {"averagePerSecond", density.averagePerSecond}};
}

nlohmann::ordered_json levelJson(const core::LevelStatistics& level, const Positioner& at) {
    return {{"durationSeconds", level.durationSeconds},
            {"averageDb", level.averageDb},
            {"peakDb", level.peakDb},
            {"peakAt", at.json(level.peakAtSeconds)},
            {"loudestSecondDb", level.loudestSecondDb},
            {"loudestSecondAt", at.json(level.loudestSecondAtSeconds)}};
}

void printColumn(const char* title, const core::DensityStatistics& density, const core::LevelStatistics& level,
                 const Positioner& at) {
    std::printf("[%s]\n", title);
    std::printf("  events:            %zu\n", density.events);
    std::printf("  max density:       %zu /s at %s\n", density.maxPerSecond, at.text(density.maxAtSeconds).c_str());
    std::printf("  average density:   %.2f /s\n", density.averagePerSecond);
    std::printf("  play time:         %s\n", formatClock(level.durationSeconds).c_str());
    std::printf("  average level:     %.1f dBFS\n", level.averageDb);
    std::printf("  peak:              %.1f dBFS at %s\n", level.peakDb, at.text(level.peakAtSeconds).c_str());
    std::printf("  loudest second:    %.1f dBFS at %s\n", level.loudestSecondDb,
                at.text(level.loudestSecondAtSeconds).c_str());
}

int runStats(const StatsArguments& arguments, const CommonOptions& options) {
    app::LocalChartStorage storage;
    app::LocalAudioSource audioSource;
    const app::ChartService charts(storage, loadModes(options.modesDirectory));
    const auto opened = charts.open(infra::pathFromUtf8(arguments.chartFile));
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }
    const auto& project = opened.chart->project;
    const auto* mode = charts.keyModeFor(project);
    if (mode == nullptr) {
        std::cerr << "unknown mode_hint: " << project.info.modeHint << "\n";
        return toInt(ExitCode::ValidationError);
    }
    const auto audio = app::loadProjectAudio(project, opened.chart->path.parent_path(), storage, audioSource);
    const auto report = app::computeChartStatistics(project, *mode, audio.byName(), arguments.sampleRate);
    const Positioner at{project.timing};

    if (options.json) {
        nlohmann::ordered_json lanes = nlohmann::ordered_json::object();
        for (const auto& [lane, count] : report.notes.perLane) {
            lanes[std::to_string(lane)] = count;
        }
        const nlohmann::ordered_json json{
            {"notes",
             {{"total", report.notes.total},
              {"normal", report.notes.normal},
              {"long", report.notes.longNotes},
              {"scratch", report.notes.scratch},
              {"scratchLong", report.notes.scratchLong},
              {"perLane", lanes}}},
            {"bgm", {{"slices", report.bgm.slices}, {"bgmColumnNotes", report.bgm.bgmColumnNotes}}},
            {"chart", {{"density", densityJson(report.chartDensity, at)}, {"level", levelJson(report.chartLevel, at)}}},
            {"background", {{"density", densityJson(report.bgmDensity, at)}, {"level", levelJson(report.bgmLevel, at)}}},
            {"total", {{"density", densityJson(report.totalDensity, at)}, {"level", levelJson(report.totalLevel, at)}}},
            {"missingAssets", report.missingAssets}};
        std::cout << json.dump(2) << "\n";
        return toInt(ExitCode::Success);
    }

    std::printf("notes: %zu (normal %zu, long %zu, scratch %zu, scratch long %zu)\n", report.notes.total,
                report.notes.normal, report.notes.longNotes, report.notes.scratch, report.notes.scratchLong);
    std::printf("bgm:   %zu slices (%zu notes in BGM columns)\n", report.bgm.slices, report.bgm.bgmColumnNotes);
    printColumn("chart", report.chartDensity, report.chartLevel, at);
    printColumn("bgm", report.bgmDensity, report.bgmLevel, at);
    printColumn("total", report.totalDensity, report.totalLevel, at);
    for (const auto& name : report.missingAssets) {
        std::cout << "missing: " << name << "\n";
    }
    return toInt(ExitCode::Success);
}

}  // namespace

void registerStatsCommand(CLI::App& app, CommonOptions& options, CommandResult& result) {
    auto arguments = std::make_shared<StatsArguments>();

    auto* stats = app.add_subcommand("stats", "Show note counts, density and loudness of a bmson (chart / BGM / total)");
    stats->add_option("chart", arguments->chartFile, "bmson file")->required();
    stats->add_option("--rate", arguments->sampleRate, "Sample rate used to measure loudness")
        ->check(CLI::Range(8000, 192000));
    stats->add_flag("--json", options.json, "Print as JSON");
    stats->callback([&, arguments] { result.exitCode = runStats(*arguments, options); });
}

}  // namespace ibs::cli
