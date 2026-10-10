#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "BmsonTestSupport.hpp"
#include "ibs/core/bmson/BmsonImporter.hpp"

namespace ibs::core::bmson {
namespace {

using testing::channel;
using testing::documentWithChannels;

bool hasNotice(const ImportResult& result, const std::string& code) {
    return std::any_of(result.notices.begin(), result.notices.end(),
                       [&](const Issue& issue) { return issue.code == code; });
}

std::vector<int> lanesAt(const Project& project, Pulse y) {
    std::vector<int> lanes;
    for (const auto& note : project.notes()) {
        if (project.absolutePulseOf(project.cutPoint(note.cutPointId)) == y) {
            lanes.push_back(note.lane);
        }
    }
    std::sort(lanes.begin(), lanes.end());
    return lanes;
}

TEST(BmsonImport, BMI001_ContinuedNotesBecomeCutPointsOfOnePlacement) {
    const auto result = importDocument(documentWithChannels(
        {channel("bass.wav", {{0, 0, 0, false}, {0, 240, 0, true}, {0, 480, 0, true}})}));

    ASSERT_EQ(result.project.placements().size(), 1u);
    EXPECT_EQ(result.project.placements()[0].startPulse, 0);
    std::vector<Pulse> offsets;
    for (const auto& cutPoint : result.project.cutPoints()) {
        offsets.push_back(cutPoint.offsetPulses);
    }
    EXPECT_EQ(offsets, (std::vector<Pulse>{0, 240, 480}));
}

TEST(BmsonImport, BMI002_RestartingNoteStartsNewPlacementOfSameAsset) {
    const auto result =
        importDocument(documentWithChannels({channel("bass.wav", {{0, 0, 0, false}, {0, 960, 0, false}})}));

    ASSERT_EQ(result.project.placements().size(), 2u);
    EXPECT_EQ(result.project.placements()[1].startPulse, 960);
    EXPECT_EQ(result.project.placements()[0].assetId, result.project.placements()[1].assetId);
}

TEST(BmsonImport, BMI003_NotesOnSamePulseShareOneCutPoint) {
    const auto result = importDocument(
        documentWithChannels({channel("chord.wav", {{0, 0, 0, false}, {1, 240, 0, true}, {7, 240, 0, true}})}));

    EXPECT_EQ(result.project.cutPoints().size(), 2u);
    EXPECT_EQ(lanesAt(result.project, 240), (std::vector<int>{1, 7}));
}

TEST(BmsonImport, BMI004_DropsBgmNoteShadowedByLaneNote) {
    const auto result = importDocument(
        documentWithChannels({channel("kick.wav", {{0, 0, 0, false}, {0, 240, 0, true}, {3, 240, 0, true}})}));

    EXPECT_EQ(lanesAt(result.project, 240), (std::vector<int>{3}));
    EXPECT_TRUE(hasNotice(result, "IMP-BGM-SHADOWED"));
}

TEST(BmsonImport, BMI005_BgmOnlyPulseBecomesCutPointWithoutNotes) {
    const auto result =
        importDocument(documentWithChannels({channel("pad.wav", {{0, 0, 0, false}, {0, 240, 0, true}})}));

    EXPECT_EQ(result.project.cutPoints().size(), 2u);
    EXPECT_TRUE(result.project.notes().empty());
}

TEST(BmsonImport, BMI006_MixedContinuationOnSamePulseRestarts) {
    const auto result = importDocument(
        documentWithChannels({channel("vox.wav", {{0, 0, 0, false}, {1, 240, 0, true}, {2, 240, 0, false}})}));

    EXPECT_EQ(result.project.placements().size(), 2u);
    EXPECT_TRUE(hasNotice(result, "IMP-MIXED-C"));
}

TEST(BmsonImport, BMI007_FirstContinuedNoteIsTreatedAsPlacementStart) {
    const auto result = importDocument(documentWithChannels({channel("vox.wav", {{1, 480, 0, true}})}));

    ASSERT_EQ(result.project.placements().size(), 1u);
    EXPECT_EQ(result.project.placements()[0].startPulse, 480);
    EXPECT_TRUE(hasNotice(result, "IMP-FIRST-CONTINUES"));
}

TEST(BmsonImport, BMI008_KeepsLaneOutsideModeAsBgmColumn) {
    const auto result =
        importDocument(documentWithChannels({channel("hat.wav", {{0, 0, 0, false}, {12, 240, 0, true}})}));

    EXPECT_EQ(lanesAt(result.project, 240), (std::vector<int>{12}));
}

TEST(BmsonImport, BMI009_SameNamedChannelsShareAssetButNotChannelGroup) {
    const auto result = importDocument(documentWithChannels(
        {channel("kick.wav", {{0, 0, 0, false}}), channel("kick.wav", {{0, 0, 0, false}})}));

    EXPECT_EQ(result.project.assets().size(), 1u);
    ASSERT_EQ(result.project.placements().size(), 2u);
    EXPECT_NE(result.project.placements()[0].channelGroup, result.project.placements()[1].channelGroup);
}

TEST(BmsonImport, BMI010_CarriesOverInfoTimingAndBga) {
    auto document = documentWithChannels({});
    document.info.artist = "artist";
    document.timing.barLines = std::vector<Pulse>{0, 960};
    document.timing.bpmEvents = {{960, 180.0}};
    document.timing.stopEvents = {{480, 240}};
    document.bga.headers = {{1, "bga.mp4"}};
    document.bga.bgaEvents = {{0, 1}};

    const auto result = importDocument(document);

    EXPECT_EQ(result.project.info, document.info);
    EXPECT_EQ(result.project.timing, document.timing);
    EXPECT_EQ(result.project.bga, document.bga);
}

}  // namespace
}  // namespace ibs::core::bmson
