#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "ibs/core/mode/KeyMode.hpp"
#include "ibs/core/model/Project.hpp"

namespace ibs::core {

[[nodiscard]] std::optional<NoteId> assignedNoteOf(const Project& project, CutPointId cutPointId);
NoteId assignLane(Project& project, CutPointId cutPointId, int lane);
std::size_t clearAssignment(Project& project, CutPointId cutPointId);

[[nodiscard]] std::vector<CutPointId> cutPointsInSongOrder(const Project& project);

struct NotePattern {
    struct Entry {
        Pulse relativePulse = 0;
        int lane = 0;
        Pulse lengthPulses = 0;
        AssetId asset;
    };
    std::vector<Entry> entries;
};

struct PasteResult {
    std::size_t added = 0;
    std::size_t alreadyPresent = 0;
    std::size_t missingCutPoints = 0;
    std::vector<NoteId> addedNotes;
};

[[nodiscard]] NotePattern copyPattern(const Project& project, const std::vector<NoteId>& notes);
PasteResult pastePattern(Project& project, const NotePattern& pattern, Pulse targetPulse);

[[nodiscard]] std::vector<int> keyLanesOf(const KeyMode& mode);
[[nodiscard]] std::map<NoteId, int> mirroredLanes(const Project& project, const std::vector<NoteId>& notes,
                                                  const KeyMode& mode);
[[nodiscard]] std::map<NoteId, int> swappedLanes(const Project& project, const std::vector<NoteId>& notes, int laneA,
                                                 int laneB);
[[nodiscard]] std::map<NoteId, int> randomizedLanes(const Project& project, const std::vector<NoteId>& notes,
                                                    const KeyMode& mode, std::uint32_t seed);

[[nodiscard]] std::vector<PlacementId> placementsDesyncedBy(const Project& project, const TimingData& before);
std::size_t keepCutTimesAfterTempoChange(Project& project, PlacementId placementId, const TimingData& before);

[[nodiscard]] std::vector<std::size_t> playableNotesPerBar(const Project& project, const KeyMode& mode);
[[nodiscard]] double recommendedTotal(std::size_t playableNotes);

}  // namespace ibs::core
