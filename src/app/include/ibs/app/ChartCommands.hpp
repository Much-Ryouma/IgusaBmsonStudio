#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ibs/app/EditorDocument.hpp"
#include "ibs/core/chart/ChartEditing.hpp"

namespace ibs::app {

std::vector<core::NoteId> assignLanes(EditorDocument& document, const std::vector<core::CutPointId>& cuts, int lane);
std::size_t clearAssignments(EditorDocument& document, const std::vector<core::CutPointId>& cuts);
void changeNoteLane(EditorDocument& document, core::NoteId note, int lane);
void changeNoteLength(EditorDocument& document, core::NoteId note, core::Pulse lengthPulses);
core::Pulse toggleLongNote(EditorDocument& document, core::NoteId note, core::Pulse lastSliceLength, core::Pulse clearance);
std::size_t removeNotes(EditorDocument& document, const std::vector<core::NoteId>& notes);
core::PasteResult pastePattern(EditorDocument& document, const core::NotePattern& pattern, core::Pulse targetPulse);
std::size_t remapLanes(EditorDocument& document, const std::string& label, const std::map<core::NoteId, int>& lanes);

struct TimingEditOutcome {
    core::TimingData before;
    std::vector<core::PlacementId> desynced;
};

TimingEditOutcome editTiming(EditorDocument& document, const std::string& label,
                             const std::function<void(core::TimingData&)>& edit);
std::size_t keepCutTimes(EditorDocument& document, const std::vector<core::PlacementId>& placements,
                         const core::TimingData& before);
void acceptTempoDrift(EditorDocument& document, const std::vector<core::PlacementId>& placements);

[[nodiscard]] core::Pulse songEndPulse(const core::Project& project);

}  // namespace ibs::app
