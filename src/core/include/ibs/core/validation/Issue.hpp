#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace ibs::core {

enum class Severity { Info, Warning, Error };

struct Issue {
    Severity severity = Severity::Info;
    std::string code;
    std::string message;
};

[[nodiscard]] inline bool hasErrors(const std::vector<Issue>& issues) {
    return std::any_of(issues.begin(), issues.end(),
                       [](const Issue& issue) { return issue.severity == Severity::Error; });
}

[[nodiscard]] inline const char* toString(Severity severity) {
    switch (severity) {
        case Severity::Info: return "info";
        case Severity::Warning: return "warning";
        case Severity::Error: return "error";
    }
    return "unknown";
}

}  // namespace ibs::core
