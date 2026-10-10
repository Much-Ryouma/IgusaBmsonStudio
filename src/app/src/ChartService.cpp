#include "ibs/app/ChartService.hpp"

#include <exception>
#include <utility>

#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/bmson/BmsonImporter.hpp"
#include "ibs/core/validation/Validator.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

ChartService::ChartService(ChartStorage& storage, core::KeyModeCatalog modes)
    : storage_(storage), modes_(std::move(modes)) {}

const core::KeyMode* ChartService::keyModeFor(const core::Project& project) const {
    return modes_.find(project.info.modeHint);
}

OpenOutcome ChartService::open(const std::filesystem::path& path) const {
    OpenOutcome outcome;
    try {
        auto parsed = storage_.load(path);
        auto imported = core::bmson::importDocument(parsed.document);

        OpenedChart chart{std::move(imported.project), path, std::move(parsed.notices)};
        chart.notices.insert(chart.notices.end(), imported.notices.begin(), imported.notices.end());
        outcome.chart = std::move(chart);
    } catch (const std::exception& error) {
        outcome.error = infra::pathToUtf8(path) + ": " + error.what();
    }
    return outcome;
}

std::vector<core::Issue> ChartService::validate(const core::Project& project,
                                                const std::filesystem::path& chartDirectory) const {
    const auto fileSystem = storage_.fileSystemAt(chartDirectory);
    return core::validateProject(project, core::ValidationContext{keyModeFor(project), fileSystem.get()});
}

SaveOutcome ChartService::save(const core::Project& project, const std::filesystem::path& path) const {
    SaveOutcome outcome;
    outcome.issues = validate(project, path.parent_path());
    if (core::hasErrors(outcome.issues)) {
        outcome.error = "検証エラーがあるため保存しませんでした";
        return outcome;
    }
    try {
        storage_.save(path, core::bmson::exportDocument(project));
        outcome.saved = true;
    } catch (const std::exception& error) {
        outcome.error = infra::pathToUtf8(path) + ": " + error.what();
    }
    return outcome;
}

}  // namespace ibs::app
