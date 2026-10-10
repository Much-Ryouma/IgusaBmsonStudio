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

}  // namespace
}  // namespace ibs::core
