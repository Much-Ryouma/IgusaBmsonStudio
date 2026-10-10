#include "ibs/app/SlicingSession.hpp"

#include <algorithm>
#include <exception>

namespace ibs::app {

SlicingSession::SlicingSession(EditorDocument& document, AudioLibrary& library)
    : document_(document), library_(library) {}

SliceSettings SlicingSession::settingsFor(core::PlacementId placementId) const {
    const auto found = settings_.find(placementId);
    return found == settings_.end() ? SliceSettings{} : found->second;
}

void SlicingSession::setSettings(core::PlacementId placementId, const SliceSettings& settings) {
    settings_[placementId] = settings;
}

SlicePreview SlicingSession::previewWith(const core::Project& project, core::PlacementId placementId,
                                         const core::OnsetAnalysis& analysis, const SliceSettings& settings) {
    SlicePreview preview;
    preview.placementId = placementId;
    preview.placementStart = project.placement(placementId).startPulse;
    const auto onsets = core::pickOnsets(analysis, settings.onset);
    preview.detectedOnsets = onsets.size();
    preview.plan = core::planCuts(onsets, analysis.sampleRate, preview.placementStart, project.timing, settings.quantize);

    // Why not 適用してから差分を取る: プレビューのたびにプロジェクト全体を複製すると、スライダー操作が重くなるため、
    // applyCutPlan と同じ規則（自動・割当なし・先頭以外で、計画にない位置）で消える切断点だけを求める
    std::set<core::Pulse> planned;
    for (const auto& cut : preview.plan.cuts) {
        planned.insert(cut.offsetPulses);
    }
    for (const auto id : project.cutPointsOf(placementId)) {
        const auto& cut = project.cutPoint(id);
        if (cut.origin == core::CutOrigin::Auto && cut.offsetPulses > 0 && !project.hasNotes(id) &&
            !planned.contains(cut.offsetPulses)) {
            preview.replaced.push_back(id);
        }
    }
    return preview;
}

std::optional<SlicePreview> SlicingSession::preview(core::PlacementId placementId, std::string* error) {
    try {
        const auto& project = document_.project();
        const auto settings = settingsFor(placementId);
        const auto& assetName = project.asset(project.placement(placementId).assetId).name;
        const auto analysis = library_.analysis(assetName, settings.onset);
        if (!analysis) {
            if (error != nullptr) {
                *error = library_.get(assetName).error;
            }
            return std::nullopt;
        }
        return previewWith(project, placementId, *analysis, settings);
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return std::nullopt;
    }
}

SliceOutcome SlicingSession::apply(const SlicePreview& preview) {
    SliceOutcome outcome;
    try {
        if (document_.project().placement(preview.placementId).startPulse != preview.placementStart) {
            outcome.error = "プレビューの後に配置が動いたため、検出し直してください";
            return outcome;
        }
        outcome.detectedOnsets = preview.detectedOnsets;
        outcome.plan = preview.plan;
        outcome.applied = document_.apply("自動切断", [&](core::Project& project) {
            return core::applyCutPlan(project, preview.placementId, preview.plan);
        });
        outcome.issues = core::attackClipIssues(document_.project(), preview.placementId, preview.plan);
        auto& clipped = clippedOffsets_[preview.placementId];
        clipped.clear();
        for (const auto& cut : preview.plan.cuts) {
            if (cut.attackClipped) {
                clipped.insert(cut.offsetPulses);
            }
        }
        outcome.succeeded = true;
    } catch (const std::exception& error) {
        outcome.error = error.what();
    }
    return outcome;
}

bool SlicingSession::isAttackClipped(core::CutPointId cutPointId) const {
    const auto& project = document_.project();
    const auto& cuts = project.cutPoints();
    const auto found = std::find_if(cuts.begin(), cuts.end(), [cutPointId](const core::CutPoint& cut) { return cut.id == cutPointId; });
    if (found == cuts.end() || found->origin != core::CutOrigin::Auto) {
        return false;
    }
    const auto clipped = clippedOffsets_.find(found->placementId);
    return clipped != clippedOffsets_.end() && clipped->second.contains(found->offsetPulses);
}

}  // namespace ibs::app
