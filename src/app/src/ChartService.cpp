#include "ibs/app/ChartService.hpp"

#include <exception>
#include <utility>

#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/bmson/BmsonImporter.hpp"
#include "ibs/core/sidecar/Sidecar.hpp"
#include "ibs/core/asset/AssetPath.hpp"
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

        OpenedChart chart{std::move(imported.project), path, std::move(parsed.notices), {}};
        chart.notices.insert(chart.notices.end(), imported.notices.begin(), imported.notices.end());
        // Why not サイドカーが読めなければ開くのを止める: サイドカーは編集の補助情報で、bmson だけでも譜面は完全に復元できるため、
        // 読めないときは通知だけ出して bmson の内容で開く
        try {
            if (const auto sidecar = storage_.loadSidecar(path)) {
                const auto applied = core::applySidecar(chart.project, *sidecar);
                chart.sliceSettings = applied.sliceSettings;
                if (applied.unmatchedPlacements > 0) {
                    chart.notices.push_back({core::Severity::Warning, "SIDECAR-MISMATCH",
                                             "サイドカーの配置 " + std::to_string(applied.unmatchedPlacements) +
                                                 " 個が bmson に見つかりません（他のツールで編集された可能性があります）"});
                }
            }
        } catch (const std::exception& error) {
            chart.notices.push_back({core::Severity::Warning, "SIDECAR-UNREADABLE",
                                     std::string("サイドカーを読めないため、bmson の内容だけで開きました: ") + error.what()});
        }
        outcome.chart = std::move(chart);
    } catch (const std::exception& error) {
        outcome.error = infra::pathToUtf8(path) + ": " + error.what();
    }
    return outcome;
}

std::vector<core::Issue> ChartService::validate(const core::Project& project,
                                                const std::filesystem::path& chartDirectory) const {
    const auto fileSystem = storage_.fileSystemAt(chartDirectory);
    core::ValidationContext context{keyModeFor(project), fileSystem.get(), std::nullopt};
    if (!project.info.bannerImage.empty() && !core::findUnsafePathReason(project.info.bannerImage)) {
        context.bannerImageSize = storage_.imageSize(chartDirectory / infra::pathFromUtf8(project.info.bannerImage));
    }
    return core::validateProject(project, context);
}

SaveOutcome ChartService::save(const core::Project& project, const std::filesystem::path& path,
                               const std::map<core::PlacementId, core::SliceSettings>& sliceSettings) const {
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
        return outcome;
    }
    try {
        storage_.saveSidecar(path, core::captureSidecar(project, sliceSettings));
    } catch (const std::exception& error) {
        outcome.issues.push_back({core::Severity::Warning, "SIDECAR-UNWRITABLE",
                                  std::string("bmson は保存しましたが、サイドカーを書けませんでした: ") + error.what()});
    }
    return outcome;
}

// Why not 検証エラーで止める: プレビュー用の一時ファイルは、作りかけの譜面（素材がまだ足りないなど）でも
// 鳴らして確かめたいため、エラーは返すだけで書き出す
SaveOutcome ChartService::exportCopy(const core::Project& project, const std::filesystem::path& path) const {
    SaveOutcome outcome;
    outcome.issues = validate(project, path.parent_path());
    try {
        storage_.save(path, core::bmson::exportDocument(project));
        outcome.saved = true;
    } catch (const std::exception& error) {
        outcome.error = infra::pathToUtf8(path) + ": " + error.what();
    }
    return outcome;
}

}  // namespace ibs::app
