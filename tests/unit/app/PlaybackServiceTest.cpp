#include <gtest/gtest.h>

#include "../support/FakeAppPorts.hpp"
#include "ibs/app/PlaybackService.hpp"

namespace ibs::app {
namespace {

using ibs::testing::FakeAudioOutput;
using ibs::testing::FilesOnlyStorage;
using ibs::testing::InMemoryAudioSource;

const std::filesystem::path kChart = "chart";

core::AudioBuffer rampOfLength(std::size_t frames, float scale) {
    core::AudioBuffer buffer{1000, 1, std::vector<float>(frames)};
    for (std::size_t index = 0; index < frames; ++index) {
        buffer.interleavedSamples[index] = scale * static_cast<float>(index);
    }
    return buffer;
}

struct Fixture {
    FilesOnlyStorage storage{{"a.wav", "b.wav"}};
    InMemoryAudioSource source;
    FakeAudioOutput output{1000};
    AudioLibrary library{storage, source};
    PlaybackService playback{library, output};
    core::Project project;
    core::AssetId assetA;
    core::AssetId assetB;
    core::PlacementId placementA;
    core::PlacementId placementB;

    Fixture() {
        source.buffers[kChart / "a.wav"] = rampOfLength(2000, 0.001f);
        source.buffers[kChart / "b.wav"] = core::AudioBuffer{1000, 1, std::vector<float>(1000, 0.5f)};
        library.setChartDirectory(kChart);
        project.timing.initialBpm = 120.0;
        project.timing.resolution = 240;
        assetA = project.addAsset("a.wav");
        assetB = project.addAsset("b.wav");
        placementA = project.addPlacement(assetA, 480, 0);
        placementB = project.addPlacement(assetB, 0, 0);
    }
};

TEST(PlaybackService, PLY001_PlaysOnlyTheSliceOfTheCutPoint) {
    Fixture fixture;
    const auto cut = fixture.project.addCutPoint(fixture.placementA, 240, core::CutOrigin::Manual);
    fixture.project.addCutPoint(fixture.placementA, 480, core::CutOrigin::Manual);

    const auto outcome = fixture.playback.playSlice(fixture.project, cut);

    ASSERT_TRUE(outcome.started) << outcome.error;
    ASSERT_EQ(fixture.output.played.frameCount(), 500u);
    EXPECT_FLOAT_EQ(fixture.output.played.interleavedSamples[0], 0.5f);
    EXPECT_FLOAT_EQ(fixture.output.played.interleavedSamples[1], 0.5f);
    EXPECT_EQ(fixture.playback.context()->kind, PlaybackKind::Slice);
    EXPECT_DOUBLE_EQ(fixture.playback.context()->songStartSeconds, 1.5);
}

TEST(PlaybackService, PLY002_PlayheadAdvancesWithTheOutput) {
    Fixture fixture;
    const auto cut = fixture.project.addCutPoint(fixture.placementA, 240, core::CutOrigin::Manual);
    static_cast<void>(fixture.playback.playSlice(fixture.project, cut));

    fixture.output.position = 250;

    EXPECT_DOUBLE_EQ(*fixture.playback.playheadSongSeconds(), 1.75);
    EXPECT_DOUBLE_EQ(*fixture.playback.playheadSourceSeconds(), 0.75);
    fixture.playback.stop();
    EXPECT_FALSE(fixture.playback.playheadSongSeconds().has_value());
}

TEST(PlaybackService, PLY003_SongMixHonoursSoloAndMute) {
    Fixture fixture;
    MixSettings mix;
    mix.muted.insert(fixture.assetA);

    ASSERT_TRUE(fixture.playback.playSong(fixture.project, 0, mix).started);
    EXPECT_EQ(fixture.output.played.frameCount(), 1000u);

    mix.soloed.insert(fixture.assetA);
    ASSERT_TRUE(fixture.playback.playSong(fixture.project, 480, mix).started);
    EXPECT_EQ(fixture.output.played.frameCount(), 2000u);
    EXPECT_FLOAT_EQ(fixture.output.played.interleavedSamples[2 * 10], 0.01f);
    EXPECT_EQ(fixture.playback.context()->kind, PlaybackKind::Song);
    EXPECT_FALSE(fixture.playback.playheadSourceSeconds().has_value());
}

TEST(PlaybackService, PLY004_ReportsWhyPlaybackCouldNotStart) {
    Fixture fixture;
    const auto missing = fixture.project.addPlacement(fixture.project.addAsset("missing.wav"), 0, 0);

    const auto noAudio = fixture.playback.playPlacement(fixture.project, missing);
    fixture.output.failStart = true;
    const auto noDevice = fixture.playback.playPlacement(fixture.project, fixture.placementA);

    EXPECT_FALSE(noAudio.started);
    EXPECT_NE(noAudio.error.find("missing.wav"), std::string::npos);
    EXPECT_FALSE(noDevice.started);
    EXPECT_FALSE(noDevice.error.empty());
    EXPECT_FALSE(fixture.playback.context().has_value());
}

}  // namespace
}  // namespace ibs::app
