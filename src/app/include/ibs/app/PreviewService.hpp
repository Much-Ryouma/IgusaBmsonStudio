#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ibs/app/ChartService.hpp"
#include "ibs/app/EditorDocument.hpp"

namespace ibs::app {

inline constexpr const char* kDefaultRenderTemplate = R"("{exe}" "{file}" --render "{output}")";

struct PlayerSettings {
    std::string executable;
    std::string commandTemplate = R"("{exe}" "{file}" -p -m {measure})";
    bool foldBgmColumns = true;
    std::string renderTemplate = kDefaultRenderTemplate;
};

struct LaunchRequest {
    std::string program;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
};

class ProcessLauncher {
public:
    virtual ~ProcessLauncher() = default;

    virtual bool launch(const LaunchRequest& request) = 0;
};

struct PreviewPosition {
    int measure = 0;
    core::Pulse pulse = 0;
    double seconds = 0.0;
};

struct PreviewOutcome {
    bool launched = false;
    std::filesystem::path file;
    std::size_t foldedBgmNotes = 0;
    LaunchRequest request;
    std::vector<core::Issue> issues;
    std::string error;
};

[[nodiscard]] std::vector<std::string> splitCommandLine(const std::string& commandLine);
[[nodiscard]] LaunchRequest buildLaunchRequest(const PlayerSettings& settings, const std::filesystem::path& file,
                                               const PreviewPosition& position);
[[nodiscard]] PreviewPosition previewPositionAt(const core::Project& project, core::Pulse cursorPulse);
[[nodiscard]] std::filesystem::path previewFileFor(const std::filesystem::path& chartPath);

class PreviewService {
public:
    PreviewService(const ChartService& charts, ProcessLauncher& launcher);

    PreviewOutcome preview(const EditorDocument& document, core::Pulse cursorPulse, const PlayerSettings& settings);

private:
    const ChartService& charts_;
    ProcessLauncher& launcher_;
};

}  // namespace ibs::app
