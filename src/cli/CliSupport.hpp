#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include "ibs/core/mode/KeyMode.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::cli {

struct CommonOptions {
    std::string modesDirectory;
    std::string midiProfilesDirectory;
    bool json = false;
};

struct CommandResult {
    int exitCode = 0;
};

[[nodiscard]] core::KeyModeCatalog loadModes(const std::string& modesDirectory);

[[nodiscard]] nlohmann::ordered_json issuesToJson(const std::vector<core::Issue>& issues);

void printIssues(const std::vector<core::Issue>& issues);

[[nodiscard]] std::string formatClock(double seconds);

void registerChartCommands(CLI::App& app, CommonOptions& options, CommandResult& result);
void registerSliceCommand(CLI::App& app, CommonOptions& options, CommandResult& result);
void registerMidiCommand(CLI::App& app, CommonOptions& options, CommandResult& result);
void registerNullTestCommand(CLI::App& app, CommonOptions& options, CommandResult& result);
void registerStatsCommand(CLI::App& app, CommonOptions& options, CommandResult& result);

}  // namespace ibs::cli
