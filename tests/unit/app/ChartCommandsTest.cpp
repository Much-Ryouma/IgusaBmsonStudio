#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/app/ChartCommands.hpp"
#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::app {
namespace {

struct Fixture {
    EditorDocument document;
    core::PlacementId placement;
    std::vector<core::CutPointId> cuts;

    Fixture() {
        core::Project project;
        project.timing.initialBpm = 120.0;
        project.timing.resolution = 240;
        placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
        cuts.push_back(project.headCutPointOf(placement));
        for (const core::Pulse offset : {240, 480, 960}) {
            cuts.push_back(project.addCutPoint(placement, offset, core::CutOrigin::Auto));
        }
        document.replace(std::move(project), std::nullopt);
    }
};

TEST(ChartCommands, CMD001_AssigningSeveralSlicesIsOneUndoStep) {
    Fixture fixture;

    const auto notes = assignLanes(fixture.document, {fixture.cuts[1], fixture.cuts[2]}, 4);

    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(fixture.document.project().note(notes[1]).lane, 4);
    EXPECT_EQ(fixture.document.undoLabel(), "レーン割当");
    fixture.document.undo();
    EXPECT_TRUE(fixture.document.project().notes().empty());
}

TEST(ChartCommands, CMD002_ClearingUnassignedSlicesAddsNoUndoStep) {
    Fixture fixture;
    assignLanes(fixture.document, {fixture.cuts[1]}, 2);

    EXPECT_EQ(clearAssignments(fixture.document, {fixture.cuts[2]}), 0u);
    EXPECT_EQ(fixture.document.undoLabel(), "レーン割当");
    EXPECT_EQ(clearAssignments(fixture.document, {fixture.cuts[1], fixture.cuts[2]}), 1u);
    EXPECT_EQ(fixture.document.undoLabel(), "割当の解除");
}

TEST(ChartCommands, CMD003_LaneAndLengthChangesSkipNoOps) {
    Fixture fixture;
    const auto note = assignLanes(fixture.document, {fixture.cuts[1]}, 2).front();

    changeNoteLane(fixture.document, note, 2);
    changeNoteLength(fixture.document, note, 0);
    EXPECT_EQ(fixture.document.undoLabel(), "レーン割当");

    changeNoteLane(fixture.document, note, 6);
    changeNoteLength(fixture.document, note, 240);
    EXPECT_EQ(fixture.document.project().note(note).lane, 6);
    EXPECT_EQ(fixture.document.project().note(note).lengthPulses, 240);
    EXPECT_EQ(fixture.document.undoLabel(), "ロングノーツの長さ");
}

TEST(ChartCommands, CMD004_PasteWithNothingToAddLeavesNoUndoStep) {
    Fixture fixture;
    const auto note = assignLanes(fixture.document, {fixture.cuts[1]}, 3).front();
    const auto pattern = core::copyPattern(fixture.document.project(), {note});

    const auto missing = pastePattern(fixture.document, pattern, 5000);
    EXPECT_EQ(missing.missingCutPoints, 1u);
    EXPECT_EQ(fixture.document.undoLabel(), "レーン割当");

    const auto pasted = pastePattern(fixture.document, pattern, 480);
    EXPECT_EQ(pasted.added, 1u);
    EXPECT_EQ(fixture.document.undoLabel(), "割当の貼り付け");
}

TEST(ChartCommands, CMD005_TimingEditMarksDesyncedPlacementsInTheSameStep) {
    Fixture fixture;

    const auto outcome = editTiming(fixture.document, "BPM の変更",
                                    [](core::TimingData& timing) { core::setBpmChange(timing, 480, 60.0); });

    EXPECT_EQ(outcome.desynced, std::vector<core::PlacementId>{fixture.placement});
    EXPECT_TRUE(fixture.document.project().tempoAffectedPlacements().contains(fixture.placement));
    EXPECT_DOUBLE_EQ(outcome.before.initialBpm, 120.0);
    EXPECT_TRUE(outcome.before.bpmEvents.empty());
    fixture.document.undo();
    EXPECT_TRUE(fixture.document.project().tempoAffectedPlacements().empty());
    EXPECT_TRUE(fixture.document.project().timing.bpmEvents.empty());
}

TEST(ChartCommands, CMD006_KeepingCutTimesOrAcceptingClearsTheWarning) {
    Fixture fixture;
    const auto outcome = editTiming(fixture.document, "BPM の変更",
                                    [](core::TimingData& timing) { core::setBpmChange(timing, 480, 60.0); });

    const auto moved = keepCutTimes(fixture.document, outcome.desynced, outcome.before);

    EXPECT_EQ(moved, 1u);
    EXPECT_EQ(fixture.document.project().cutPoint(fixture.cuts[3]).offsetPulses, 720);
    EXPECT_TRUE(fixture.document.project().tempoAffectedPlacements().empty());
    fixture.document.undo();
    acceptTempoDrift(fixture.document, outcome.desynced);
    EXPECT_EQ(fixture.document.project().cutPoint(fixture.cuts[3]).offsetPulses, 960);
    EXPECT_TRUE(fixture.document.project().tempoAffectedPlacements().empty());
}

TEST(ChartCommands, CMD007_UnchangedTimingAddsNoUndoStepAndSongEndCoversContent) {
    Fixture fixture;

    const auto outcome = editTiming(fixture.document, "BPM の変更",
                                    [](core::TimingData& timing) { core::setBpmChange(timing, 0, 120.0); });

    EXPECT_FALSE(fixture.document.canUndo());
    EXPECT_TRUE(outcome.desynced.empty());
    EXPECT_EQ(songEndPulse(fixture.document.project()), 960 + 4 * 960);
}

}  // namespace
}  // namespace ibs::app
