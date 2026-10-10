#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/app/ArrangementCommands.hpp"

namespace ibs::app {
namespace {

struct Fixture {
    EditorDocument document;
    core::PlacementId placement;

    Fixture() {
        core::Project project;
        placement = project.addPlacement(project.addAsset("bass.wav"), 960, 0);
        document.replace(std::move(project), std::nullopt);
    }
};

TEST(ArrangementCommands, ARR001_ManualCutIsPlacedAtTheSongPulse) {
    Fixture fixture;

    const auto cut = addManualCut(fixture.document, fixture.placement, 1200);

    EXPECT_EQ(fixture.document.project().cutPoint(cut).offsetPulses, 240);
    EXPECT_EQ(fixture.document.project().cutPoint(cut).origin, core::CutOrigin::Manual);
    EXPECT_THROW(addManualCut(fixture.document, fixture.placement, 960), std::invalid_argument);
    fixture.document.undo();
    EXPECT_EQ(fixture.document.project().cutPoints().size(), 1u);
}

TEST(ArrangementCommands, ARR002_MovingToTheSamePulseAddsNoUndoStep) {
    Fixture fixture;
    const auto cut = addManualCut(fixture.document, fixture.placement, 1200);

    moveCut(fixture.document, cut, 1200);
    EXPECT_EQ(fixture.document.undoLabel(), "切断点の追加");

    moveCut(fixture.document, cut, 1260);
    EXPECT_EQ(fixture.document.project().cutPoint(cut).offsetPulses, 300);
    EXPECT_EQ(fixture.document.undoLabel(), "切断点の移動");
}

TEST(ArrangementCommands, ARR003_RemovesOnlyRemovableCutsInOneStep) {
    Fixture fixture;
    const auto head = fixture.document.project().headCutPointOf(fixture.placement);
    const auto assigned = addManualCut(fixture.document, fixture.placement, 1200);
    fixture.document.apply("ノーツ", [&](core::Project& project) { project.addNote(assigned, 1, 0); });
    const auto first = addManualCut(fixture.document, fixture.placement, 1440);
    const auto second = addManualCut(fixture.document, fixture.placement, 1680);

    const auto removed = removeCuts(fixture.document, {head, assigned, first, second});

    EXPECT_EQ(removed, 2u);
    EXPECT_EQ(fixture.document.project().cutPoints().size(), 2u);
    fixture.document.undo();
    EXPECT_EQ(fixture.document.project().cutPoints().size(), 4u);
    EXPECT_EQ(removeCuts(fixture.document, {head}), 0u);
    EXPECT_EQ(fixture.document.undoLabel(), "切断点の追加");
}

TEST(ArrangementCommands, ARR004_DuplicateCopiesCutsButNotNotes) {
    Fixture fixture;
    const auto cut = addManualCut(fixture.document, fixture.placement, 1200);
    fixture.document.apply("ノーツ", [&](core::Project& project) { project.addNote(cut, 1, 0); });

    const auto copy = duplicatePlacement(fixture.document, fixture.placement, 3840);

    const auto& project = fixture.document.project();
    const auto copiedCuts = project.cutPointsOf(copy);
    ASSERT_EQ(copiedCuts.size(), 2u);
    EXPECT_EQ(project.absolutePulseOf(project.cutPoint(copiedCuts[1])), 4080);
    EXPECT_EQ(project.notes().size(), 1u);
}

TEST(ArrangementCommands, ARR005_UndoingPlacementRemovalRestoresTheSameIds) {
    Fixture fixture;
    const auto cut = addManualCut(fixture.document, fixture.placement, 1200);
    movePlacement(fixture.document, fixture.placement, 960);
    EXPECT_EQ(fixture.document.undoLabel(), "切断点の追加");

    removePlacement(fixture.document, fixture.placement);
    EXPECT_TRUE(fixture.document.project().placements().empty());
    fixture.document.undo();

    EXPECT_EQ(fixture.document.project().cutPoint(cut).placementId, fixture.placement);
}

TEST(ArrangementCommands, ARR006_SnapsSelectedCutsToTheGridInOneStep) {
    Fixture fixture;
    const auto nearBeat = addManualCut(fixture.document, fixture.placement, 1205);
    const auto onGrid = addManualCut(fixture.document, fixture.placement, 1320);
    const auto collides = addManualCut(fixture.document, fixture.placement, 1316);
    const auto head = fixture.document.project().headCutPointOf(fixture.placement);

    const auto moved = snapCutsToGrid(fixture.document, {nearBeat, onGrid, collides, head}, 16);

    EXPECT_EQ(moved, 1u);
    EXPECT_EQ(fixture.document.project().cutPoint(nearBeat).offsetPulses, 240);
    EXPECT_EQ(fixture.document.project().cutPoint(collides).offsetPulses, 356);
    fixture.document.undo();
    EXPECT_EQ(fixture.document.project().cutPoint(nearBeat).offsetPulses, 245);
}

}  // namespace
}  // namespace ibs::app
