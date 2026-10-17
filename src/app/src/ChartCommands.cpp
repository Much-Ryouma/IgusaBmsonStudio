#include "ibs/app/ChartCommands.hpp"

#include <algorithm>
#include <stdexcept>

namespace ibs::app {

namespace {

constexpr int kTrailingBars = 4;

}  // namespace

std::vector<core::NoteId> assignLanes(EditorDocument& document, const std::vector<core::CutPointId>& cuts, int lane) {
    if (cuts.empty()) {
        return {};
    }
    return document.apply("レーン割当", [&](core::Project& project) {
        std::vector<core::NoteId> notes;
        for (const auto cut : cuts) {
            notes.push_back(core::assignLane(project, cut, lane));
        }
        return notes;
    });
}

std::size_t clearAssignments(EditorDocument& document, const std::vector<core::CutPointId>& cuts) {
    const auto& current = document.project();
    const bool anyAssigned = std::any_of(cuts.begin(), cuts.end(), [&](core::CutPointId cut) {
        return current.containsCutPoint(cut) && !current.notesOf(cut).empty();
    });
    if (!anyAssigned) {
        return 0;
    }
    return document.apply("割当の解除", [&](core::Project& project) {
        std::size_t removed = 0;
        for (const auto cut : cuts) {
            if (project.containsCutPoint(cut)) {
                removed += core::clearAssignment(project, cut);
            }
        }
        return removed;
    });
}

void changeNoteLane(EditorDocument& document, core::NoteId note, int lane) {
    if (document.project().note(note).lane == lane) {
        return;
    }
    document.apply("レーンの変更", [&](core::Project& project) { project.remapNoteLanes({{note, lane}}); });
}

core::Pulse toggleLongNote(EditorDocument& document, core::NoteId note, core::Pulse lastSliceLength, core::Pulse clearance) {
    const auto length = document.project().note(note).lengthPulses > 0
                            ? core::Pulse{0}
                            : core::longNoteLengthFor(document.project(), note, lastSliceLength, clearance);
    if (length == 0 && document.project().note(note).lengthPulses == 0) {
        throw std::invalid_argument("同じレーンのすぐ上に次のノーツがあるため、ロングノーツにできません");
    }
    document.apply("ロングノーツの切り替え", [&](core::Project& project) { project.setNoteLength(note, length); });
    return length;
}

void changeNoteLength(EditorDocument& document, core::NoteId note, core::Pulse lengthPulses) {
    if (document.project().note(note).lengthPulses == lengthPulses) {
        return;
    }
    document.apply("ロングノーツの長さ", [&](core::Project& project) { project.setNoteLength(note, lengthPulses); });
}

std::size_t removeNotes(EditorDocument& document, const std::vector<core::NoteId>& notes) {
    std::vector<core::NoteId> existing;
    std::copy_if(notes.begin(), notes.end(), std::back_inserter(existing),
                 [&](core::NoteId id) { return document.project().containsNote(id); });
    if (existing.empty()) {
        return 0;
    }
    document.apply("ノーツの削除", [&](core::Project& project) {
        for (const auto id : existing) {
            project.removeNote(id);
        }
    });
    return existing.size();
}

core::PasteResult pastePattern(EditorDocument& document, const core::NotePattern& pattern, core::Pulse targetPulse) {
    core::Project trial = document.project();
    const auto preview = core::pastePattern(trial, pattern, targetPulse);
    // Why not 何も足せなくても履歴に積む: 貼り付け先に切断点がないときに、空の「元に戻す」が増えて紛らわしいため
    if (preview.added == 0) {
        return preview;
    }
    return document.apply("割当の貼り付け", [&](core::Project& project) { return core::pastePattern(project, pattern, targetPulse); });
}

std::size_t remapLanes(EditorDocument& document, const std::string& label, const std::map<core::NoteId, int>& lanes) {
    std::map<core::NoteId, int> changes;
    for (const auto& [id, lane] : lanes) {
        if (document.project().note(id).lane != lane) {
            changes.emplace(id, lane);
        }
    }
    if (changes.empty()) {
        return 0;
    }
    document.apply(label, [&](core::Project& project) { project.remapNoteLanes(changes); });
    return changes.size();
}

TimingEditOutcome editTiming(EditorDocument& document, const std::string& label,
                             const std::function<void(core::TimingData&)>& edit) {
    TimingEditOutcome outcome;
    outcome.before = document.project().timing;
    core::TimingData trial = outcome.before;
    edit(trial);
    if (trial == outcome.before) {
        return outcome;
    }
    outcome.desynced = document.apply(label, [&](core::Project& project) {
        edit(project.timing);
        const auto desynced = core::placementsDesyncedBy(project, outcome.before);
        for (const auto placement : desynced) {
            project.markTempoAffected(placement);
        }
        return desynced;
    });
    return outcome;
}

std::size_t keepCutTimes(EditorDocument& document, const std::vector<core::PlacementId>& placements,
                         const core::TimingData& before) {
    if (placements.empty()) {
        return 0;
    }
    return document.apply("切断点の時間を保つ", [&](core::Project& project) {
        std::size_t moved = 0;
        for (const auto placement : placements) {
            moved += core::keepCutTimesAfterTempoChange(project, placement, before);
        }
        return moved;
    });
}

void acceptTempoDrift(EditorDocument& document, const std::vector<core::PlacementId>& placements) {
    if (placements.empty()) {
        return;
    }
    document.apply("テンポ変更の警告を消す", [&](core::Project& project) {
        for (const auto placement : placements) {
            project.clearTempoAffected(placement);
        }
    });
}

core::Pulse songEndPulse(const core::Project& project) {
    core::Pulse end = 0;
    for (const auto& cut : project.cutPoints()) {
        end = std::max(end, project.absolutePulseOf(cut));
    }
    for (const auto& note : project.notes()) {
        end = std::max(end, project.absolutePulseOf(project.cutPoint(note.cutPointId)) + note.lengthPulses);
    }
    if (project.timing.barLines && !project.timing.barLines->empty()) {
        end = std::max(end, project.timing.barLines->back());
    }
    return end + static_cast<core::Pulse>(std::max(1, project.timing.resolution)) * 4 * kTrailingBars;
}

}  // namespace ibs::app
