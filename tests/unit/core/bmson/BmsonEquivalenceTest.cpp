#include <algorithm>

#include <gtest/gtest.h>

#include "BmsonTestSupport.hpp"
#include "ibs/core/bmson/BmsonEquivalence.hpp"
#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/bmson/BmsonImporter.hpp"

namespace ibs::core::bmson {
namespace {

using testing::channel;
using testing::documentWithChannels;

bool mentions(const std::vector<std::string>& differences, const std::string& text) {
    return std::any_of(differences.begin(), differences.end(),
                       [&](const std::string& difference) { return difference.find(text) != std::string::npos; });
}

TEST(BmsonEquivalence, BEQ001_IgnoresOrderOfNotesAndChannels) {
    const auto expected = documentWithChannels({channel("a.wav", {{0, 0, 0, false}, {1, 240, 0, true}}),
                                                channel("b.wav", {{0, 0, 0, false}})});
    const auto actual = documentWithChannels({channel("b.wav", {{0, 0, 0, false}}),
                                              channel("a.wav", {{1, 240, 0, true}, {0, 0, 0, false}})});

    EXPECT_TRUE(describePlaybackDifferences(expected, actual).empty());
}

TEST(BmsonEquivalence, BEQ002_IgnoresBgmNoteThatPlayersDiscard) {
    const auto expected = documentWithChannels({channel("a.wav", {{0, 0, 0, false}, {0, 240, 0, true}, {3, 240, 0, true}})});
    const auto actual = documentWithChannels({channel("a.wav", {{0, 0, 0, false}, {3, 240, 0, true}})});

    EXPECT_TRUE(describePlaybackDifferences(expected, actual).empty());
}

TEST(BmsonEquivalence, BEQ003_ReportsDifferentLane) {
    const auto expected = documentWithChannels({channel("a.wav", {{0, 0, 0, false}, {1, 240, 0, true}})});
    const auto actual = documentWithChannels({channel("a.wav", {{0, 0, 0, false}, {2, 240, 0, true}})});

    EXPECT_TRUE(mentions(describePlaybackDifferences(expected, actual), "sound_channels"));
}

TEST(BmsonEquivalence, BEQ004_ReportsDifferentTitle) {
    auto expected = documentWithChannels({});
    auto actual = expected;
    actual.info.title = "other";

    EXPECT_TRUE(mentions(describePlaybackDifferences(expected, actual), "info.title"));
}

TEST(BmsonEquivalence, BEQ005_ExportImportExportIsStable) {
    Project project;
    project.timing.barLines = std::vector<Pulse>{0, 960};
    const AssetId asset = project.addAsset("kick.wav");
    const PlacementId first = project.addPlacement(asset, 0, 0);
    project.addNote(project.addCutPoint(first, 240, CutOrigin::Auto), 1, 0);
    project.addNote(project.addCutPoint(first, 480, CutOrigin::Auto), 12, 0);
    const PlacementId second = project.addPlacement(asset, 960, 0);
    project.addNote(project.headCutPointOf(second), 8, 240);

    const auto once = exportDocument(project);
    const auto twice = exportDocument(importDocument(once).project);

    EXPECT_TRUE(describePlaybackDifferences(once, twice).empty());
}

}  // namespace
}  // namespace ibs::core::bmson
