#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/core/chart/CutLayout.hpp"

namespace ibs::core {
namespace {

struct Song {
    Project project;
    AssetId drums = project.addAsset("drums.wav");
    AssetId hats = project.addAsset("hats.wav");
    AssetId other = project.addAsset("other.wav");
    AssetId single = project.addAsset("single.wav");
    PlacementId drumsLate = project.addPlacement(drums, 3840, 0);
    PlacementId drumsEarly = project.addPlacement(drums, 0, 0);
    PlacementId hatsEarly = project.addPlacement(hats, 0, 1);
    PlacementId hatsLate = project.addPlacement(hats, 3840, 1);
    PlacementId otherEarly = project.addPlacement(other, 0, 2);
    PlacementId otherLate = project.addPlacement(other, 1920, 2);
    PlacementId singleOnly = project.addPlacement(single, 0, 3);
};

std::vector<Pulse> offsetsOf(const Project& project, PlacementId placement) {
    std::vector<Pulse> offsets;
    for (const auto cut : project.cutPointsOf(placement)) {
        offsets.push_back(project.cutPoint(cut).offsetPulses);
    }
    return offsets;
}

TEST(CutLayout, CLY001_ListsAssetsPlacedAtExactlyTheSameStarts) {
    Song song;

    EXPECT_EQ(placementStartsOf(song.project, song.drums), (std::vector<Pulse>{0, 3840}));
    EXPECT_EQ(assetsWithSamePlacements(song.project, song.drums), std::vector<AssetId>{song.hats});
    EXPECT_TRUE(assetsWithSamePlacements(song.project, song.single).empty());
}

TEST(CutLayout, CLY002_CopiesCutPositionsAndClearsTheTargetAssignments) {
    Song song;
    song.project.addCutPoint(song.drumsEarly, 240, CutOrigin::Auto);
    song.project.addCutPoint(song.drumsEarly, 480, CutOrigin::Manual);
    song.project.addCutPoint(song.drumsLate, 120, CutOrigin::Auto);
    song.project.addNote(song.project.cutPointsOf(song.drumsEarly)[1], 1, 0);
    const auto oldCut = song.project.addCutPoint(song.hatsEarly, 360, CutOrigin::Auto);
    song.project.addNote(oldCut, 3, 0);
    song.project.addNote(song.project.headCutPointOf(song.hatsLate), 5, 0);
    song.project.markTempoAffected(song.hatsEarly);

    const auto copied = copyCutLayout(song.project, song.drums, song.hats);

    EXPECT_EQ(copied.placements, 2u);
    EXPECT_EQ(copied.copiedCuts, 3u);
    EXPECT_EQ(copied.removedNotes, 2u);
    EXPECT_EQ(offsetsOf(song.project, song.hatsEarly), (std::vector<Pulse>{0, 240, 480}));
    EXPECT_EQ(offsetsOf(song.project, song.hatsLate), (std::vector<Pulse>{0, 120}));
    EXPECT_EQ(song.project.cutPoint(song.project.cutPointsOf(song.hatsEarly)[2]).origin, CutOrigin::Manual);
    EXPECT_EQ(song.project.notes().size(), 1u);
    EXPECT_FALSE(song.project.tempoAffectedPlacements().contains(song.hatsEarly));
}

TEST(CutLayout, CLY003_RefusesAssetsPlacedDifferently) {
    Song song;
    const auto before = song.project.cutPoints().size();

    EXPECT_THROW(copyCutLayout(song.project, song.drums, song.other), std::invalid_argument);
    EXPECT_THROW(copyCutLayout(song.project, song.drums, song.drums), std::invalid_argument);
    EXPECT_EQ(song.project.cutPoints().size(), before);
}

}  // namespace
}  // namespace ibs::core
