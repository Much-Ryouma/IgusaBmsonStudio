#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ibs/app/ChartStorage.hpp"
#include "ibs/core/mode/KeyMode.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::app {

struct OpenedChart {
    core::Project project;
    std::filesystem::path path;
    std::vector<core::Issue> notices;
};

struct OpenOutcome {
    std::optional<OpenedChart> chart;
    std::string error;
};

struct SaveOutcome {
    bool saved = false;
    std::vector<core::Issue> issues;
    std::string error;
};

class ChartService {
public:
    ChartService(ChartStorage& storage, core::KeyModeCatalog modes);

    [[nodiscard]] OpenOutcome open(const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<core::Issue> validate(const core::Project& project,
                                                    const std::filesystem::path& chartDirectory) const;
    [[nodiscard]] SaveOutcome save(const core::Project& project, const std::filesystem::path& path) const;

    [[nodiscard]] const core::KeyMode* keyModeFor(const core::Project& project) const;

private:
    ChartStorage& storage_;
    core::KeyModeCatalog modes_;
};

}  // namespace ibs::app
