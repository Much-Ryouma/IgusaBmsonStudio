#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ibs/app/AudioLibrary.hpp"
#include "ibs/app/EditorDocument.hpp"
#include "ibs/app/SlicingService.hpp"

namespace ibs::app {

struct SlicePreview {
    core::PlacementId placementId;
    core::Pulse placementStart = 0;
    std::size_t detectedOnsets = 0;
    core::CutPlan plan;
    std::vector<core::CutPointId> replaced;
};

class SlicingSession {
public:
    SlicingSession(EditorDocument& document, AudioLibrary& library);

    [[nodiscard]] SliceSettings settingsFor(core::PlacementId placementId) const;
    void setSettings(core::PlacementId placementId, const SliceSettings& settings);
    [[nodiscard]] const std::map<core::PlacementId, SliceSettings>& allSettings() const noexcept { return settings_; }
    void replaceAllSettings(std::map<core::PlacementId, SliceSettings> settings);

    [[nodiscard]] static SlicePreview previewWith(const core::Project& project, core::PlacementId placementId,
                                                  const core::OnsetAnalysis& analysis, const SliceSettings& settings);
    [[nodiscard]] std::optional<SlicePreview> preview(core::PlacementId placementId, std::string* error = nullptr);

    SliceOutcome apply(const SlicePreview& preview);

    [[nodiscard]] bool isAttackClipped(core::CutPointId cutPointId) const;

private:
    EditorDocument& document_;
    AudioLibrary& library_;
    std::map<core::PlacementId, SliceSettings> settings_;
    std::map<core::PlacementId, std::set<core::Pulse>> clippedOffsets_;
};

}  // namespace ibs::app
