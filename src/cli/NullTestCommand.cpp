#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "CliSupport.hpp"
#include "ExitCode.hpp"
#include "ibs/app/NullTestService.hpp"
#include "ibs/infra/audio/AudioFileDecoder.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::cli {

namespace {

struct NullTestArguments {
    std::string chartFile;
    std::string renderedFile;
    std::string player;
    std::string renderTemplate = app::kDefaultRenderTemplate;
    double thresholdDb = -30.0;
    std::size_t worstCount = 8;
    int sampleRate = 44100;
};

void printReport(const app::NullTestReport& report, double thresholdDb) {
    std::printf("residual: %.1f dB (threshold %.1f dB) -> %s\n", report.residualDb, thresholdDb,
                report.passed ? "PASS" : "FAIL");
    std::printf("offset:   %.2f ms\ngain:     %.3f\n", report.offsetMs, report.gain);
    for (const auto& name : report.missingAssets) {
        std::cout << "missing:  " << name << "\n";
    }
    if (!report.worst.empty()) {
        std::cout << "worst sections:\n";
    }
    for (const auto& section : report.worst) {
        std::printf("  %s  bar %d  %.1f dB\n", formatClock(section.startSeconds).c_str(), section.bar,
                    section.residualDb);
    }
}

nlohmann::ordered_json reportToJson(const app::NullTestReport& report) {
    nlohmann::ordered_json worst = nlohmann::ordered_json::array();
    for (const auto& section : report.worst) {
        worst.push_back({{"startSeconds", section.startSeconds},
                         {"bar", section.bar},
                         {"pulse", section.pulse},
                         {"residualDb", section.residualDb}});
    }
    return {{"passed", report.passed},       {"residualDb", report.residualDb},
            {"offsetMs", report.offsetMs},   {"gain", report.gain},
            {"worst", worst},                {"missingAssets", report.missingAssets}};
}

int runNullTest(const NullTestArguments& arguments, const CommonOptions& options) {
    app::LocalChartStorage storage;
    app::LocalAudioSource audio;
    const app::ChartService charts(storage, loadModes(options.modesDirectory));
    const auto opened = charts.open(infra::pathFromUtf8(arguments.chartFile));
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }

    app::NullTestOptions testOptions;
    testOptions.thresholdDb = arguments.thresholdDb;
    testOptions.worstCount = arguments.worstCount;
    testOptions.internalSampleRate = arguments.sampleRate;

    const app::NullTestService service(charts, storage, audio);
    const auto& chart = *opened.chart;
    const auto directory = chart.path.parent_path();
    app::NullTestReport report;
    if (!arguments.renderedFile.empty()) {
        const auto rendered = infra::decodeAudioFile(infra::pathFromUtf8(arguments.renderedFile));
        report = service.compareWithRendering(chart.project, directory, rendered, testOptions);
    } else if (!arguments.player.empty()) {
        app::LocalProcessRunner runner;
        app::PlayerSettings player;
        player.executable = arguments.player;
        report = service.compareWithPlayer(chart.project, chart.path, player, arguments.renderTemplate, runner,
                                           testOptions);
    } else {
        report = service.compareWithInternalPlayer(chart.project, directory, testOptions);
    }

    if (!report.completed) {
        std::cerr << report.error << "\n";
        return toInt(ExitCode::IoError);
    }
    if (options.json) {
        std::cout << reportToJson(report).dump(2) << "\n";
    } else {
        printReport(report, arguments.thresholdDb);
    }
    return toInt(report.passed ? ExitCode::Success : ExitCode::ValidationError);
}

}  // namespace

void registerNullTestCommand(CLI::App& app, CommonOptions& options, CommandResult& result) {
    auto arguments = std::make_shared<NullTestArguments>();

    auto* nulltest = app.add_subcommand(
        "nulltest", "Compare the placed stems with how the bmson plays back (internal player, a rendered wav, or a player)");
    nulltest->add_option("chart", arguments->chartFile, "bmson file")->required();
    auto* rendered = nulltest->add_option("--rendered", arguments->renderedFile,
                                          "Audio rendered by a player from the same bmson");
    nulltest->add_option("--player", arguments->player, "Player executable that can render to wav (e.g. Pulsus)")
        ->excludes(rendered);
    nulltest->add_option("--render-template", arguments->renderTemplate,
                         "Command line for rendering ({exe} {file} {output})");
    nulltest->add_option("--threshold", arguments->thresholdDb, "Pass when the residual is below this (dB)");
    nulltest->add_option("--worst", arguments->worstCount, "Number of worst sections to list");
    nulltest->add_option("--rate", arguments->sampleRate, "Sample rate of the internal player")
        ->check(CLI::Range(8000, 192000));
    nulltest->add_flag("--json", options.json, "Print as JSON");
    nulltest->callback([&, arguments] { result.exitCode = runNullTest(*arguments, options); });
}

}  // namespace ibs::cli
