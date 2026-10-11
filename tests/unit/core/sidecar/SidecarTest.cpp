#include <gtest/gtest.h>

#include "ibs/core/model/BgaEditing.hpp"
#include "ibs/core/sidecar/Sidecar.hpp"

namespace ibs::core {
namespace {

struct Edited {
    Project project;
    PlacementId first;
    PlacementId second;
    CutPointId autoCut;
    CutPointId manualCut;

    Edited() {
        const auto bass = project.addAsset("bass.wav");
        first = project.addPlacement(bass, 0, 0);
        second = project.addPlacement(bass, 1920, 0);
        autoCut = project.addCutPoint(first, 240, CutOrigin::Auto);
        manualCut = project.addCutPoint(first, 480, CutOrigin::Manual);
        project.markTempoAffected(second);
    }
};

Project reimported() {
    Project project;
    const auto bass = project.addAsset("bass.wav");
    const auto first = project.addPlacement(bass, 0, 0);
    project.addPlacement(bass, 1920, 0);
    project.addCutPoint(first, 240, CutOrigin::Imported);
    project.addCutPoint(first, 480, CutOrigin::Imported);
    return project;
}

TEST(Sidecar, SDC001_RestoresOriginsTempoMarksAndSliceSettingsAfterReopening) {
    Edited edited;
    SliceSettings coarse;
    coarse.quantize.divisionsPerWholeNote = 8;
    const auto sidecar = captureSidecar(edited.project, {{edited.first, coarse}});

    Project opened = reimported();
    const auto result = applySidecar(opened, sidecar);

    EXPECT_EQ(result.matchedPlacements, 2u);
    EXPECT_EQ(result.restoredOrigins, 2u);
    const auto first = opened.placements()[0].id;
    const auto cuts = opened.cutPointsOf(first);
    EXPECT_EQ(opened.cutPoint(cuts[1]).origin, CutOrigin::Auto);
    EXPECT_EQ(opened.cutPoint(cuts[2]).origin, CutOrigin::Manual);
    EXPECT_TRUE(opened.tempoAffectedPlacements().contains(opened.placements()[1].id));
    ASSERT_EQ(result.sliceSettings.size(), 1u);
    EXPECT_EQ(result.sliceSettings.at(first).quantize.divisionsPerWholeNote, 8);
}

TEST(Sidecar, SDC002_IgnoresEntriesThatNoLongerMatchTheBmson) {
    Edited edited;
    const auto sidecar = captureSidecar(edited.project, {});
    Project changed;
    const auto bass = changed.addAsset("bass.wav");
    const auto moved = changed.addPlacement(bass, 960, 0);
    changed.addCutPoint(moved, 240, CutOrigin::Imported);

    const auto result = applySidecar(changed, sidecar);

    EXPECT_EQ(result.matchedPlacements, 0u);
    EXPECT_EQ(result.unmatchedPlacements, 2u);
    EXPECT_EQ(changed.cutPoint(changed.cutPointsOf(moved)[1]).origin, CutOrigin::Imported);
}

TEST(Sidecar, SDC003_WritesOnlyCutsThatAreNotPlainImports) {
    Edited edited;

    const auto sidecar = captureSidecar(edited.project, {});

    ASSERT_EQ(sidecar.placements.size(), 2u);
    ASSERT_EQ(sidecar.placements[0].cuts.size(), 2u);
    EXPECT_EQ(sidecar.placements[0].cuts[0].offsetPulses, 240);
    EXPECT_EQ(sidecar.placements[0].cuts[1].origin, CutOrigin::Manual);
    EXPECT_TRUE(sidecar.placements[1].cuts.empty());
}

TEST(BgaEditing, BGE001_AddsHeadersWithNewIdsAndReusesTheSameFile) {
    Bga bga;
    bga.headers.push_back(BgaHeader{5, "intro.png"});

    EXPECT_EQ(addBgaHeader(bga, "main.png"), 6u);
    EXPECT_EQ(addBgaHeader(bga, "intro.png"), 5u);
    EXPECT_EQ(bga.headers.size(), 2u);
    EXPECT_THROW(addBgaHeader(bga, ""), std::invalid_argument);
}

TEST(BgaEditing, BGE002_PlacesReplacesMovesAndRemovesEventsPerTrack) {
    Bga bga;
    const auto image = addBgaHeader(bga, "a.png");
    const auto other = addBgaHeader(bga, "b.png");

    placeBgaEvent(bga, BgaTrack::Bga, 960, image);
    placeBgaEvent(bga, BgaTrack::Bga, 0, image);
    placeBgaEvent(bga, BgaTrack::Bga, 960, other);
    placeBgaEvent(bga, BgaTrack::Poor, 960, image);

    EXPECT_EQ(bga.bgaEvents, (std::vector<BgaEvent>{{0, image}, {960, other}}));
    EXPECT_EQ(bga.poorEvents.size(), 1u);
    EXPECT_THROW(placeBgaEvent(bga, BgaTrack::Layer, 0, 99), std::invalid_argument);
    EXPECT_THROW(moveBgaEvent(bga, BgaTrack::Bga, 0, 960), std::invalid_argument);
    moveBgaEvent(bga, BgaTrack::Bga, 960, 1920);
    EXPECT_EQ(bga.bgaEvents.back(), (BgaEvent{1920, other}));
    removeBgaEvent(bga, BgaTrack::Bga, 0);
    EXPECT_THROW(removeBgaEvent(bga, BgaTrack::Bga, 0), std::invalid_argument);
}

TEST(BgaEditing, BGE003_RemovingAHeaderRemovesItsEvents) {
    Bga bga;
    const auto image = addBgaHeader(bga, "a.png");
    const auto other = addBgaHeader(bga, "b.png");
    placeBgaEvent(bga, BgaTrack::Bga, 0, image);
    placeBgaEvent(bga, BgaTrack::Layer, 0, other);

    removeBgaHeader(bga, image);

    EXPECT_EQ(bga.headers.size(), 1u);
    EXPECT_TRUE(bga.bgaEvents.empty());
    EXPECT_EQ(bga.layerEvents.size(), 1u);
    EXPECT_THROW(removeBgaHeader(bga, image), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
