#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "CliSupport.hpp"
#include "ExitCode.hpp"
#include "ibs/app/ChartService.hpp"
#include "ibs/core/midi/MidiExporter.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/midi/StandardMidiFile.hpp"

namespace ibs::cli {

namespace {

struct MidiArguments {
    std::string chartFile;
    std::string output;
    std::string profileId = "punai-punai-ensou";
    std::optional<int> ppq;
    std::optional<core::MidiTimeOrigin> origin;
    bool tempoOnly = false;
};

core::MidiProfile findProfile(const std::string& profileId, const std::string& directory) {
    const auto profilesDirectory =
        directory.empty() ? infra::executableDirectory() / "midi_profiles" : infra::pathFromUtf8(directory);
    for (auto& profile : infra::loadMidiProfiles(profilesDirectory)) {
        if (profile.id == profileId) {
            return profile;
        }
    }
    throw std::invalid_argument("MIDI profile not found: " + profileId + " (in " + infra::pathToUtf8(profilesDirectory) + ")");
}

int runMidi(const MidiArguments& arguments, const CommonOptions& options) {
    auto profile = findProfile(arguments.profileId, options.midiProfilesDirectory);
    if (arguments.ppq) {
        profile.ppq = *arguments.ppq;
    }
    if (arguments.origin) {
        profile.origin = *arguments.origin;
    }

    app::LocalChartStorage storage;
    const app::ChartService service(storage, loadModes(options.modesDirectory));
    const auto opened = service.open(infra::pathFromUtf8(arguments.chartFile));
    if (!opened.chart) {
        std::cerr << opened.error << "\n";
        return toInt(ExitCode::IoError);
    }

    const auto exported = arguments.tempoOnly ? core::buildTempoMapMidi(opened.chart->project, profile)
                                              : core::buildCutPointMidi(opened.chart->project, profile);
    const auto output = infra::pathFromUtf8(arguments.output);
    const bool writesDirectory = exported.files.size() > 1 || std::filesystem::is_directory(output);
    if (writesDirectory) {
        std::filesystem::create_directories(output);
    }

    nlohmann::ordered_json written = nlohmann::ordered_json::array();
    for (const auto& file : exported.files) {
        const auto path = writesDirectory ? output / infra::pathFromUtf8(file.suggestedName) : output;
        infra::writeTextFileAtomically(path, infra::serializeStandardMidiFile(file.sequence));
        written.push_back(infra::pathToUtf8(path));
    }

    if (options.json) {
        std::cout << nlohmann::ordered_json{{"profile", profile.id}, {"files", written},
                                            {"issues", issuesToJson(exported.issues)}}
                         .dump(2)
                  << "\n";
    } else {
        for (const auto& path : written) {
            std::cout << "wrote " << path.get<std::string>() << "\n";
        }
        printIssues(exported.issues);
    }
    return toInt(ExitCode::Success);
}

}  // namespace

void registerMidiCommand(CLI::App& app, CommonOptions& options, CommandResult& result) {
    auto arguments = std::make_shared<MidiArguments>();
    const std::map<std::string, core::MidiTimeOrigin> origins{{"song", core::MidiTimeOrigin::SongStart},
                                                              {"placement", core::MidiTimeOrigin::PlacementStart}};

    auto* midi = app.add_subcommand("midi", "Export cut points (or the tempo map) of a bmson as MIDI");
    midi->add_option("chart", arguments->chartFile, "bmson file")->required();
    midi->add_option("-o,--out", arguments->output, "Output .mid file (a folder when one file per placement)")
        ->required();
    midi->add_option("--profile", arguments->profileId, "MIDI profile id (punai-punai-ensou, generic)");
    midi->add_option("--ppq", arguments->ppq, "Override ticks per quarter note")->check(CLI::Range(1, 32767));
    midi->add_option("--origin", arguments->origin, "Time origin: song (default of the profile) or placement")
        ->transform(CLI::CheckedTransformer(origins, CLI::ignore_case))
        ->option_text("TEXT");
    midi->add_flag("--tempo-only", arguments->tempoOnly, "Write only the tempo map (for importing into a DAW)");
    midi->add_flag("--json", options.json, "Print as JSON");
    midi->callback([&, arguments] { result.exitCode = runMidi(*arguments, options); });
}

}  // namespace ibs::cli
