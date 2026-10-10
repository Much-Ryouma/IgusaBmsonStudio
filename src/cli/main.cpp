#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include "ExitCode.hpp"
#include "ibs/app/ChartService.hpp"
#include "ibs/core/BuildInfo.hpp"
#include "ibs/core/bmson/BmsonEquivalence.hpp"
#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/validation/Validator.hpp"
#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/mode/KeyModeJsonLoader.hpp"

namespace {

using ibs::cli::ExitCode;
using ibs::cli::toInt;
using Json = nlohmann::ordered_json;

struct CommonOptions {
    std::string modesDirectory;
    bool json = false;
};

ibs::core::KeyModeCatalog loadModes(const std::string& modesDirectory) {
    const auto directory = modesDirectory.empty() ? ibs::infra::executableDirectory() / "modes"
                                                  : ibs::infra::pathFromUtf8(modesDirectory);
    if (!std::filesystem::is_directory(directory)) {
        std::cerr << "warning: mode definitions not found: " << ibs::infra::pathToUtf8(directory) << "\n";
        return {};
    }
    return ibs::infra::loadKeyModeCatalog(directory);
}

Json issuesToJson(const std::vector<ibs::core::Issue>& issues) {
    Json array = Json::array();
    for (const auto& issue : issues) {
        array.push_back({{"severity", ibs::core::toString(issue.severity)}, {"code", issue.code}, {"message", issue.message}});
    }
    return array;
}

void printIssues(const std::vector<ibs::core::Issue>& issues) {
    for (const auto& issue : issues) {
        std::cout << "[" << ibs::core::toString(issue.severity) << "] " << issue.code << ": " << issue.message << "\n";
    }
}

int runInfo(const std::string& file, const CommonOptions& options) {
    ibs::app::LocalChartStorage storage;
    const ibs::app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto opened = service.open(ibs::infra::pathFromUtf8(file));
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }
    const auto& project = opened.chart->project;
    const auto* keyMode = service.keyModeFor(project);

    Json info;
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
        info["playable_notes"] = ibs::core::countPlayableNotes(project, *keyMode);
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
    ibs::app::LocalChartStorage storage;
    const ibs::app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto path = ibs::infra::pathFromUtf8(file);
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
    return toInt(ibs::core::hasErrors(issues) ? ExitCode::ValidationError : ExitCode::Success);
}

int runConvert(const std::string& input, const std::string& output, bool check, const CommonOptions& options) {
    ibs::app::LocalChartStorage storage;
    const ibs::app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto inputPath = ibs::infra::pathFromUtf8(input);
    const auto opened = service.open(inputPath);
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }
    printIssues(opened.chart->notices);

    const auto saved = service.save(opened.chart->project, ibs::infra::pathFromUtf8(output));
    printIssues(saved.issues);
    if (!saved.saved) {
        std::cerr << saved.error << "\n";
        return toInt(saved.issues.empty() ? ExitCode::IoError : ExitCode::ValidationError);
    }
    if (!check) {
        return toInt(ExitCode::Success);
    }

    const auto original = ibs::infra::loadBmsonFile(inputPath).document;
    const auto written = ibs::infra::loadBmsonFile(ibs::infra::pathFromUtf8(output)).document;
    const auto differences = ibs::core::bmson::describePlaybackDifferences(original, written);
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

int main(int argc, char** argv) {
    const std::string versionText =
        std::string(ibs::core::kApplicationName) + " " + std::string(ibs::core::kVersion);

    CLI::App app{versionText + " command line tool", "igusa-cli"};
    app.set_version_flag("--version", versionText);
    app.require_subcommand(0, 1);

    CommonOptions options;
    app.add_option("--modes-dir", options.modesDirectory, "Directory of mode definitions (default: <exe>/modes)");

    std::string infoFile;
    auto* info = app.add_subcommand("info", "Show a summary of a bmson file");
    info->add_option("file", infoFile, "bmson file")->required();
    info->add_flag("--json", options.json, "Print as JSON");

    std::string validateFile;
    auto* validate = app.add_subcommand("validate", "Validate a bmson file (exit code 1 on errors)");
    validate->add_option("file", validateFile, "bmson file")->required();
    validate->add_flag("--json", options.json, "Print as JSON");

    std::string convertInput;
    std::string convertOutput;
    bool convertCheck = false;
    auto* convert = app.add_subcommand("convert", "Read a bmson file and write it back through the editor model");
    convert->add_option("input", convertInput, "Input bmson file")->required();
    convert->add_option("output", convertOutput, "Output bmson file")->required();
    convert->add_flag("--check", convertCheck, "Verify the output plays the same as the input");

    try {
        app.parse(argc, argv);
    } catch (const CLI::Success& success) {
        return app.exit(success);
    } catch (const CLI::ParseError& error) {
        app.exit(error);
        return toInt(ExitCode::UsageError);
    }

    try {
        if (info->parsed()) {
            return runInfo(infoFile, options);
        }
        if (validate->parsed()) {
            return runValidate(validateFile, options);
        }
        if (convert->parsed()) {
            return runConvert(convertInput, convertOutput, convertCheck, options);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return toInt(ExitCode::IoError);
    }

    std::cout << app.help();
    return toInt(ExitCode::Success);
}
