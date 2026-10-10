#include <algorithm>

#include <gtest/gtest.h>

#include "ibs/core/bmson/BmsonExporter.hpp"

namespace ibs::core::bmson {
namespace {

TEST(BmsonExport, BME001_HeadRestartsAndOtherCutPointsContinue) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    project.addCutPoint(placement, 240, CutOrigin::Auto);

    const auto document = exportDocument(project);

    ASSERT_EQ(document.soundChannels.size(), 1u);
    const auto& notes = document.soundChannels[0].notes;
    ASSERT_EQ(notes.size(), 2u);
    EXPECT_FALSE(notes[0].c);
    EXPECT_TRUE(notes[1].c);
}

TEST(BmsonExport, BME002_UnassignedCutPointIsWrittenAsBgmNote) {
    Project project;
    project.addPlacement(project.addAsset("pad.wav"), 480, 0);

    const auto document = exportDocument(project);

    EXPECT_EQ(document.soundChannels[0].notes, (std::vector<Note>{{kBgmLane, 480, 0, false}}));
}

TEST(BmsonExport, BME003_AssignedCutPointWritesOnlyLaneNotes) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("chord.wav"), 0, 0);
    const CutPointId cutPoint = project.addCutPoint(placement, 240, CutOrigin::Manual);
    project.addNote(cutPoint, 7, 0);
    project.addNote(cutPoint, 1, 0);

    const auto document = exportDocument(project);

    EXPECT_EQ(document.soundChannels[0].notes,
              (std::vector<Note>{{kBgmLane, 0, 0, false}, {1, 240, 0, true}, {7, 240, 0, true}}));
}

TEST(BmsonExport, BME004_NonOverlappingPlacementsShareOneChannel) {
    Project project;
    const AssetId asset = project.addAsset("fill.wav");
    const PlacementId first = project.addPlacement(asset, 0, 0);
    project.addCutPoint(first, 240, CutOrigin::Manual);
    project.addPlacement(asset, 960, 0);

    const auto document = exportDocument(project);

    ASSERT_EQ(document.soundChannels.size(), 1u);
    const auto& notes = document.soundChannels[0].notes;
    EXPECT_EQ(std::count_if(notes.begin(), notes.end(), [](const Note& note) { return !note.c; }), 2);
}

TEST(BmsonExport, BME005_OverlappingPlacementsAreSplitIntoChannels) {
    Project project;
    const AssetId asset = project.addAsset("fill.wav");
    const PlacementId first = project.addPlacement(asset, 0, 0);
    project.addCutPoint(first, 1200, CutOrigin::Manual);
    project.addPlacement(asset, 960, 0);

    const auto document = exportDocument(project);

    ASSERT_EQ(document.soundChannels.size(), 2u);
    EXPECT_EQ(document.soundChannels[0].name, "fill.wav");
    EXPECT_EQ(document.soundChannels[1].name, "fill.wav");
}

TEST(BmsonExport, BME006_WritesLongNoteLength) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("lead.wav"), 0, 0);
    project.addNote(project.headCutPointOf(placement), 4, 480);

    EXPECT_EQ(exportDocument(project).soundChannels[0].notes[0].l, 480);
}

TEST(BmsonExport, BME007_WritesVersionAndSortsNotesByPulseThenLane) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("lead.wav"), 0, 0);
    const CutPointId late = project.addCutPoint(placement, 480, CutOrigin::Manual);
    const CutPointId early = project.addCutPoint(placement, 240, CutOrigin::Manual);
    project.addNote(late, 2, 0);
    project.addNote(early, 5, 0);
    project.addNote(early, 3, 0);

    const auto document = exportDocument(project);

    EXPECT_EQ(document.version, "1.0.0");
    const auto& notes = document.soundChannels[0].notes;
    EXPECT_TRUE(std::is_sorted(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    }));
}

}  // namespace
}  // namespace ibs::core::bmson
