#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

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

    try {
        app.parse(argc, argv);
    } catch (const CLI::Success& success) {
        return app.exit(success);
    } catch (const CLI::ParseError& error) {
        app.exit(error);
        return toInt(ExitCode::UsageError);
    }

    if (app.get_subcommands().empty()) {
        std::cout << app.help();
    }
    return toInt(ExitCode::Success);
}
