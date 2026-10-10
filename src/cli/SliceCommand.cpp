#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "CliSupport.hpp"
#include "ExitCode.hpp"
#include "ibs/app/SlicingService.hpp"
#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/infra/audio/AudioFileDecoder.hpp"
#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::cli {

namespace {

struct SliceArguments {
    std::string audioFile;
    std::string chartFile;
    double bpm = 120.0;
    int resolution = core::kDefaultResolution;
    core::Pulse startPulse = 0;
    core::OnsetFunction function = core::OnsetFunction::SpectralFlux;
    core::OnsetParameters onset;
    int grid = 16;
    double strength = 1.0;
    std::optional<double> windowMs;
    double attackClipWarningMs = 10.0;
    std::string bmsonOutput;
};

core::TimingData timingFor(const SliceArguments& arguments) {
    if (!arguments.chartFile.empty()) {
        return infra::loadBmsonFile(infra::pathFromUtf8(arguments.chartFile)).document.timing;
    }
    core::TimingData timing;
    timing.initialBpm = arguments.bpm;
    timing.resolution = arguments.resolution;
    return timing;
}

std::string formatNumber(double value, int decimals) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
}

int runSlice(const SliceArguments& arguments, const CommonOptions& options) {
    const auto audioPath = infra::pathFromUtf8(arguments.audioFile);
    const auto audio = infra::decodeAudioFile(audioPath);

    core::Project project;
    project.timing = timingFor(arguments);
    project.info.title = infra::pathToUtf8(audioPath.stem());
    const auto placement =
        project.addPlacement(project.addAsset(infra::pathToUtf8(audioPath.filename())), arguments.startPulse, 0);

    app::SliceSettings settings;
    settings.onset = arguments.onset;
    settings.onset.function = arguments.function;
    settings.quantize.divisionsPerWholeNote = arguments.grid;
    settings.quantize.strength = arguments.strength;
    settings.quantize.windowMs = arguments.windowMs;
    settings.quantize.attackClipWarningMs = arguments.attackClipWarningMs;
    const auto outcome = app::SlicingService::sliceBuffer(project, placement, audio, settings);

    if (options.json) {
        nlohmann::ordered_json output;
        output["audio"] = arguments.audioFile;
        output["sample_rate"] = audio.sampleRate;
        output["duration_seconds"] = audio.durationSeconds();
        output["detected_onsets"] = outcome.detectedOnsets;
        output["merged_into_head"] = outcome.plan.mergedIntoHead;
        output["merged_duplicates"] = outcome.plan.mergedDuplicates;
        nlohmann::ordered_json cuts = nlohmann::ordered_json::array();
        for (const auto& cut : outcome.plan.cuts) {
            cuts.push_back({{"y", arguments.startPulse + cut.offsetPulses},
                            {"offset", cut.offsetPulses},
                            {"onset_seconds", cut.onsetSeconds},
                            {"late_ms", cut.lateMs},
                            {"snapped", cut.snapped},
                            {"attack_clipped", cut.attackClipped}});
        }
        output["cuts"] = std::move(cuts);
        output["issues"] = issuesToJson(outcome.issues);
        std::cout << output.dump(2) << "\n";
    } else {
        std::cout << "detected " << outcome.detectedOnsets << " onsets, " << outcome.plan.cuts.size()
                  << " cut points (merged into head: " << outcome.plan.mergedIntoHead
                  << ", merged duplicates: " << outcome.plan.mergedDuplicates << ")\n";
        std::cout << "y\tonset[s]\tlate[ms]\tsnapped\n";
        for (const auto& cut : outcome.plan.cuts) {
            std::cout << arguments.startPulse + cut.offsetPulses << "\t" << formatNumber(cut.onsetSeconds, 4) << "\t"
                      << formatNumber(cut.lateMs, 1) << "\t" << (cut.snapped ? "yes" : "no")
                      << (cut.attackClipped ? "\tattack clipped" : "") << "\n";
        }
        printIssues(outcome.issues);
    }

    if (!arguments.bmsonOutput.empty()) {
        infra::saveBmsonFile(infra::pathFromUtf8(arguments.bmsonOutput), core::bmson::exportDocument(project));
    }
    return toInt(ExitCode::Success);
}

}  // namespace

void registerSliceCommand(CLI::App& app, CommonOptions& options, CommandResult& result) {
    auto arguments = std::make_shared<SliceArguments>();
    const std::map<std::string, core::OnsetFunction> functions{{"flux", core::OnsetFunction::SpectralFlux},
                                                               {"energy", core::OnsetFunction::Energy},
                                                               {"hfc", core::OnsetFunction::HighFrequencyContent}};

    auto* slice = app.add_subcommand("slice", "Detect onsets in an audio file and plan cut points");
    slice->add_option("audio", arguments->audioFile, "Audio file (wav / ogg / flac / mp3)")->required();
    slice->add_option("--chart", arguments->chartFile, "Take tempo, stops and bar lines from this bmson");
    slice->add_option("--bpm", arguments->bpm, "Tempo when --chart is not given")->check(CLI::PositiveNumber);
    slice->add_option("--resolution", arguments->resolution, "Pulses per quarter note when --chart is not given")
        ->check(CLI::PositiveNumber);
    slice->add_option("--start", arguments->startPulse, "Pulse where the audio starts playing")
        ->check(CLI::NonNegativeNumber);
    slice->add_option("--function", arguments->function, "Detection function: flux (default), energy, hfc")
        ->transform(CLI::CheckedTransformer(functions, CLI::ignore_case))
        ->option_text("TEXT");
    slice->add_option("--sensitivity", arguments->onset.sensitivity, "0 (few cuts) .. 1 (many cuts)")
        ->check(CLI::Range(0.0, 1.0));
    slice->add_option("--min-interval", arguments->onset.minimumIntervalMs, "Minimum gap between cuts [ms]")
        ->check(CLI::NonNegativeNumber);
    slice->add_option("--gate", arguments->onset.silenceGateDb, "Ignore frames quieter than this [dBFS]");
    slice->add_option("--preroll", arguments->onset.prerollMs, "Place cuts this much before the attack [ms]")
        ->check(CLI::NonNegativeNumber);
    slice->add_option("--grid", arguments->grid, "Quantize grid per whole note (16 = 16th notes, 0 = off)")
        ->check(CLI::NonNegativeNumber);
    slice->add_option("--strength", arguments->strength, "Quantize strength 0..1")->check(CLI::Range(0.0, 1.0));
    slice->add_option("--window", arguments->windowMs, "Snap only when the grid is within this distance [ms]");
    slice->add_option("--attack-warning", arguments->attackClipWarningMs,
                      "Warn when a cut lands this much after the attack [ms]");
    slice->add_option("--bmson", arguments->bmsonOutput, "Also write a bmson with the cut points as BGM notes");
    slice->add_flag("--json", options.json, "Print as JSON");
    slice->callback([&, arguments] { result.exitCode = runSlice(*arguments, options); });
}

}  // namespace ibs::cli
