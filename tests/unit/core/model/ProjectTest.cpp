#include <set>
#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/core/model/Project.hpp"

namespace ibs::core {
namespace {

TEST(Project, PRJ001_AssignsDistinctValidIdsToEveryEntity) {
    Project project;
    const AssetId asset = project.addAsset("kick.wav");
    const PlacementId placement = project.addPlacement(asset, 0, 0);
    const CutPointId cutPoint = project.addCutPoint(placement, 240, CutOrigin::Manual);
    const NoteId note = project.addNote(cutPoint, 1, 0);

    const std::set<std::uint64_t> ids{asset.value, placement.value, cutPoint.value, note.value,
                                      project.headCutPointOf(placement).value};
    EXPECT_EQ(ids.size(), 5u);
    EXPECT_EQ(ids.count(0), 0u);
}

TEST(Project, PRJ002_PlacementCreatesHeadCutPointAtOffsetZero) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 960, 0);

    const CutPoint& head = project.cutPoint(project.headCutPointOf(placement));
    EXPECT_EQ(head.offsetPulses, 0);
    EXPECT_EQ(head.placementId, placement);
}

TEST(Project, PRJ003_CutPointPositionIsPlacementStartPlusOffset) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 960, 0);
    const CutPointId cutPoint = project.addCutPoint(placement, 240, CutOrigin::Auto);

    EXPECT_EQ(project.absolutePulseOf(project.cutPoint(cutPoint)), 1200);
}

TEST(Project, PRJ004_RejectsDuplicatedAssetName) {
    Project project;
    project.addAsset("kick.wav");

    EXPECT_THROW(project.addAsset("kick.wav"), std::invalid_argument);
}

TEST(Project, PRJ005_RejectsDuplicatedCutPointOffset) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("kick.wav"), 0, 0);
    project.addCutPoint(placement, 240, CutOrigin::Manual);

    EXPECT_THROW(project.addCutPoint(placement, 240, CutOrigin::Manual), std::invalid_argument);
}

TEST(Project, PRJ006_RejectsCutPointAtOrBeforePlacementHead) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("kick.wav"), 0, 0);

    EXPECT_THROW(project.addCutPoint(placement, 0, CutOrigin::Manual), std::invalid_argument);
    EXPECT_THROW(project.addCutPoint(placement, -1, CutOrigin::Manual), std::invalid_argument);
}

TEST(Project, PRJ007_RejectsNoteOnBgmLane) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("kick.wav"), 0, 0);

    EXPECT_THROW(project.addNote(project.headCutPointOf(placement), kBgmLane, 0), std::invalid_argument);
}

TEST(Project, PRJ008_RejectsSecondNoteOnSameLaneOfSameCutPoint) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("kick.wav"), 0, 0);
    const CutPointId head = project.headCutPointOf(placement);
    project.addNote(head, 3, 0);

    EXPECT_THROW(project.addNote(head, 3, 0), std::invalid_argument);
}

TEST(Project, PRJ009_RejectsUnknownId) {
    Project project;

    EXPECT_THROW(static_cast<void>(project.asset(AssetId{42})), std::invalid_argument);
}

TEST(Project, PRJ010_AllowsOneSliceOnSeveralLanes) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("chord.wav"), 0, 0);
    const CutPointId head = project.headCutPointOf(placement);

    project.addNote(head, 1, 0);
    project.addNote(head, 7, 0);

    EXPECT_EQ(project.notes().size(), 2u);
}

TEST(Project, PRJ011_RemovesOnlyUnassignedNonHeadCutPoints) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("kick.wav"), 0, 0);
    const CutPointId assigned = project.addCutPoint(placement, 240, CutOrigin::Auto);
    project.addNote(assigned, 1, 0);
    const CutPointId free = project.addCutPoint(placement, 480, CutOrigin::Auto);

    EXPECT_THROW(project.removeCutPoint(project.headCutPointOf(placement)), std::invalid_argument);
    EXPECT_THROW(project.removeCutPoint(assigned), std::invalid_argument);
    project.removeCutPoint(free);
    EXPECT_EQ(project.cutPoints().size(), 2u);
}

TEST(Project, PRJ012_MovedCutPointKeepsItsNotesAndBecomesManual) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 960, 0);
    const CutPointId cut = project.addCutPoint(placement, 240, CutOrigin::Auto);
    const NoteId note = project.addNote(cut, 3, 0);

    project.moveCutPoint(cut, 300);

    EXPECT_EQ(project.absolutePulseOf(project.cutPoint(cut)), 1260);
    EXPECT_EQ(project.cutPoint(cut).origin, CutOrigin::Manual);
    ASSERT_EQ(project.notes().size(), 1u);
    EXPECT_EQ(project.notes().front().id, note);
    EXPECT_EQ(project.notes().front().cutPointId, cut);
}

TEST(Project, PRJ013_RejectsInvalidCutPointMoves) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const CutPointId first = project.addCutPoint(placement, 240, CutOrigin::Manual);
    project.addCutPoint(placement, 480, CutOrigin::Manual);

    EXPECT_THROW(project.moveCutPoint(project.headCutPointOf(placement), 120), std::invalid_argument);
    EXPECT_THROW(project.moveCutPoint(first, 0), std::invalid_argument);
    EXPECT_THROW(project.moveCutPoint(first, 480), std::invalid_argument);
    EXPECT_EQ(project.cutPoint(first).offsetPulses, 240);
}

TEST(Project, PRJ014_MovingPlacementMovesAllItsCutPoints) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const CutPointId cut = project.addCutPoint(placement, 240, CutOrigin::Manual);

    project.movePlacement(placement, 1920);

    EXPECT_EQ(project.absolutePulseOf(project.cutPoint(project.headCutPointOf(placement))), 1920);
    EXPECT_EQ(project.absolutePulseOf(project.cutPoint(cut)), 2160);
    EXPECT_THROW(project.movePlacement(placement, -1), std::invalid_argument);
}

TEST(Project, PRJ015_RemovingPlacementRemovesItsCutPointsAndNotesOnly) {
    Project project;
    const AssetId asset = project.addAsset("bass.wav");
    const PlacementId removed = project.addPlacement(asset, 0, 0);
    project.addNote(project.addCutPoint(removed, 240, CutOrigin::Manual), 1, 0);
    const PlacementId kept = project.addPlacement(asset, 1920, 0);
    const NoteId keptNote = project.addNote(project.addCutPoint(kept, 240, CutOrigin::Manual), 2, 0);

    project.removePlacement(removed);

    ASSERT_EQ(project.placements().size(), 1u);
    EXPECT_EQ(project.placements().front().id, kept);
    EXPECT_EQ(project.cutPoints().size(), 2u);
    ASSERT_EQ(project.notes().size(), 1u);
    EXPECT_EQ(project.notes().front().id, keptNote);
    EXPECT_EQ(project.assets().size(), 1u);
}

TEST(Project, PRJ016_ListsCutPointsOfPlacementInTimeOrder) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const CutPointId late = project.addCutPoint(placement, 720, CutOrigin::Manual);
    const CutPointId early = project.addCutPoint(placement, 240, CutOrigin::Manual);
    project.addPlacement(project.addAsset("lead.wav"), 0, 0);

    const std::vector<CutPointId> expected{project.headCutPointOf(placement), early, late};
    EXPECT_EQ(project.cutPointsOf(placement), expected);
}

TEST(Project, PRJ017_EditsAndRemovesNotes) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const CutPointId cut = project.addCutPoint(placement, 240, CutOrigin::Auto);
    const NoteId note = project.addNote(cut, 1, 0);

    project.setNoteLength(note, 480);
    EXPECT_EQ(project.note(note).lengthPulses, 480);
    EXPECT_THROW(project.setNoteLength(note, -1), std::invalid_argument);
    EXPECT_EQ(project.notesOf(cut), std::vector<NoteId>{note});

    project.removeNote(note);
    EXPECT_FALSE(project.containsNote(note));
    EXPECT_THROW(project.removeNote(note), std::invalid_argument);
}

TEST(Project, PRJ018_RemapsLanesAsOneChangeAndRejectsCollisions) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const CutPointId cut = project.addCutPoint(placement, 240, CutOrigin::Auto);
    const NoteId first = project.addNote(cut, 1, 0);
    const NoteId second = project.addNote(cut, 2, 0);

    project.remapNoteLanes({{first, 2}, {second, 1}});
    EXPECT_EQ(project.note(first).lane, 2);
    EXPECT_EQ(project.note(second).lane, 1);

    EXPECT_THROW(project.remapNoteLanes({{first, 1}}), std::invalid_argument);
    EXPECT_THROW(project.remapNoteLanes({{first, 0}}), std::invalid_argument);
    EXPECT_EQ(project.note(first).lane, 2);
}

TEST(Project, PRJ019_RetimingKeepsTheCutOrigin) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const CutPointId cut = project.addCutPoint(placement, 240, CutOrigin::Auto);

    project.retimeCutPoint(cut, 250);

    EXPECT_EQ(project.cutPoint(cut).offsetPulses, 250);
    EXPECT_EQ(project.cutPoint(cut).origin, CutOrigin::Auto);
}

TEST(Project, PRJ020_TempoAffectedMarkFollowsThePlacement) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);

    project.markTempoAffected(placement);
    EXPECT_TRUE(project.tempoAffectedPlacements().contains(placement));
    project.removePlacement(placement);

    EXPECT_TRUE(project.tempoAffectedPlacements().empty());
    EXPECT_THROW(project.markTempoAffected(placement), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
