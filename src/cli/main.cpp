#include <exception>
#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

#include "CliSupport.hpp"
#include "ExitCode.hpp"
#include "ibs/core/BuildInfo.hpp"

int main(int argc, char** argv) {
    using ibs::cli::ExitCode;
    using ibs::cli::toInt;

    const std::string versionText =
        std::string(ibs::core::kApplicationName) + " " + std::string(ibs::core::kVersion);

    CLI::App app{versionText + " command line tool", "igusa-cli"};
    app.set_version_flag("--version", versionText);
    app.require_subcommand(0, 1);

    ibs::cli::CommonOptions options;
    ibs::cli::CommandResult result;
    app.add_option("--modes-dir", options.modesDirectory, "Directory of mode definitions (default: <exe>/modes)");
    app.add_option("--midi-profiles-dir", options.midiProfilesDirectory,
                   "Directory of MIDI profiles (default: <exe>/midi_profiles)");

    ibs::cli::registerChartCommands(app, options, result);
    ibs::cli::registerSliceCommand(app, options, result);
    ibs::cli::registerMidiCommand(app, options, result);
    ibs::cli::registerNullTestCommand(app, options, result);
    ibs::cli::registerStatsCommand(app, options, result);

    try {
        app.parse(argc, argv);
    } catch (const CLI::Success& success) {
        return app.exit(success);
    } catch (const CLI::ParseError& error) {
        app.exit(error);
        return toInt(ExitCode::UsageError);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return toInt(ExitCode::IoError);
    }

    if (app.get_subcommands().empty()) {
        std::cout << app.help();
    }
    return result.exitCode;
}
