#include "CliSupport.hpp"

#include <filesystem>
#include <iostream>

#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/mode/KeyModeJsonLoader.hpp"

namespace ibs::cli {

core::KeyModeCatalog loadModes(const std::string& modesDirectory) {
    const auto directory = modesDirectory.empty() ? infra::executableDirectory() / "modes"
                                                  : infra::pathFromUtf8(modesDirectory);
    if (!std::filesystem::is_directory(directory)) {
        std::cerr << "warning: mode definitions not found: " << infra::pathToUtf8(directory) << "\n";
        return {};
    }
    return infra::loadKeyModeCatalog(directory);
}

nlohmann::ordered_json issuesToJson(const std::vector<core::Issue>& issues) {
    nlohmann::ordered_json array = nlohmann::ordered_json::array();
    for (const auto& issue : issues) {
        array.push_back(
            {{"severity", core::toString(issue.severity)}, {"code", issue.code}, {"message", issue.message}});
    }
    return array;
}

void printIssues(const std::vector<core::Issue>& issues) {
    for (const auto& issue : issues) {
        std::cout << "[" << core::toString(issue.severity) << "] " << issue.code << ": " << issue.message << "\n";
    }
}

}  // namespace ibs::cli
