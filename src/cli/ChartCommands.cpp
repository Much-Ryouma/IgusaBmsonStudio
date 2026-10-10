#include <iostream>
#include <string>

#include "CliSupport.hpp"
#include "ExitCode.hpp"
#include "ibs/app/ChartService.hpp"
#include "ibs/core/bmson/BmsonEquivalence.hpp"
#include "ibs/core/validation/Validator.hpp"
#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::cli {

namespace {

struct ChartArguments {
    std::string infoFile;
    std::string validateFile;
    std::string convertInput;
    std::string convertOutput;
    bool convertCheck = false;
};

int runInfo(const std::string& file, const CommonOptions& options) {
    app::LocalChartStorage storage;
    const app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto opened = service.open(infra::pathFromUtf8(file));
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }
    const auto& project = opened.chart->project;
    const auto* keyMode = service.keyModeFor(project);

    nlohmann::ordered_json info;
    info["title"] = project.info.title;
    info["artist"] = project.info.artist;
    info["chart_name"] = project.info.chartName;
    info["mode_hint"] = project.info.modeHint;
    info["level"] = project.info.level;
    info["init_bpm"] = project.timing.initialBpm;
    info["resolution"] = project.timing.resolution;
    info["assets"] = project.assets().size();
    info["placements"] = project.placements().size();
    info["cut_points"] = project.cutPoints().size();
    info["notes"] = project.notes().size();
    if (keyMode != nullptr) {
        info["playable_notes"] = core::countPlayableNotes(project, *keyMode);
    }

    if (options.json) {
        std::cout << info.dump(2) << "\n";
    } else {
        for (const auto& [key, value] : info.items()) {
            std::cout << key << ": " << (value.is_string() ? value.get<std::string>() : value.dump()) << "\n";
        }
    }
    return toInt(ExitCode::Success);
}

int runValidate(const std::string& file, const CommonOptions& options) {
    app::LocalChartStorage storage;
    const app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto path = infra::pathFromUtf8(file);
    const auto opened = service.open(path);
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }
    auto issues = opened.chart->notices;
    const auto validation = service.validate(opened.chart->project, path.parent_path());
    issues.insert(issues.end(), validation.begin(), validation.end());

    if (options.json) {
        std::cout << issuesToJson(issues).dump(2) << "\n";
    } else {
        printIssues(issues);
    }
    return toInt(core::hasErrors(issues) ? ExitCode::ValidationError : ExitCode::Success);
}

int runConvert(const ChartArguments& arguments, const CommonOptions& options) {
    app::LocalChartStorage storage;
    const app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto inputPath = infra::pathFromUtf8(arguments.convertInput);
    const auto opened = service.open(inputPath);
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }
    printIssues(opened.chart->notices);

    const auto outputPath = infra::pathFromUtf8(arguments.convertOutput);
    const auto saved = service.save(opened.chart->project, outputPath);
    printIssues(saved.issues);
    if (!saved.saved) {
        std::cerr << saved.error << "\n";
        return toInt(saved.issues.empty() ? ExitCode::IoError : ExitCode::ValidationError);
    }
    if (!arguments.convertCheck) {
        return toInt(ExitCode::Success);
    }

    const auto differences = core::bmson::describePlaybackDifferences(infra::loadBmsonFile(inputPath).document,
                                                                      infra::loadBmsonFile(outputPath).document);
    if (differences.empty()) {
        std::cout << "check: 再生内容は元のファイルと一致しています\n";
        return toInt(ExitCode::Success);
    }
    for (const auto& difference : differences) {
        std::cout << "check: " << difference << "\n";
    }
    return toInt(ExitCode::ValidationError);
}

}  // namespace

void registerChartCommands(CLI::App& app, CommonOptions& options, CommandResult& result) {
    auto arguments = std::make_shared<ChartArguments>();

    auto* info = app.add_subcommand("info", "Show a summary of a bmson file");
    info->add_option("file", arguments->infoFile, "bmson file")->required();
    info->add_flag("--json", options.json, "Print as JSON");
    info->callback([&, arguments] { result.exitCode = runInfo(arguments->infoFile, options); });

    auto* validate = app.add_subcommand("validate", "Validate a bmson file (exit code 1 on errors)");
    validate->add_option("file", arguments->validateFile, "bmson file")->required();
    validate->add_flag("--json", options.json, "Print as JSON");
    validate->callback([&, arguments] { result.exitCode = runValidate(arguments->validateFile, options); });

    auto* convert = app.add_subcommand("convert", "Read a bmson file and write it back through the editor model");
    convert->add_option("input", arguments->convertInput, "Input bmson file")->required();
    convert->add_option("output", arguments->convertOutput, "Output bmson file")->required();
    convert->add_flag("--check", arguments->convertCheck, "Verify the output plays the same as the input");
    convert->callback([&, arguments] { result.exitCode = runConvert(*arguments, options); });
}

}  // namespace ibs::cli
