#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ibs/core/playback/PlaybackRange.hpp"

namespace ibs::core {
namespace {

constexpr int kRate = 1000;

AudioBuffer monoBuffer(std::vector<float> samples, int sampleRate = kRate) {
    return AudioBuffer{sampleRate, 1, std::move(samples)};
}

Project projectAt120Bpm() {
    Project project;
    project.timing.initialBpm = 120.0;
    project.timing.resolution = 240;
    return project;
}

TEST(PlaybackRange, RNG001_SliceRangeRunsFromItsCutToTheNextCut) {
    Project project = projectAt120Bpm();
    const PlacementId placement = project.addPlacement(project.addAsset("a.wav"), 480, 0);
    const CutPointId first = project.addCutPoint(placement, 240, CutOrigin::Manual);
    const CutPointId last = project.addCutPoint(placement, 720, CutOrigin::Manual);

    EXPECT_EQ(sliceRange(project, project.headCutPointOf(placement), kRate, 3000), (SampleRange{0, 500}));
    EXPECT_EQ(sliceRange(project, first, kRate, 3000), (SampleRange{500, 1500}));
    EXPECT_EQ(sliceRange(project, last, kRate, 3000), (SampleRange{1500, 3000}));
    EXPECT_EQ(sliceRange(project, last, kRate, 1200), (SampleRange{1200, 1200}));
}

TEST(PlaybackRange, RNG002_SampleOffsetFollowsTempoChanges) {
    Project project = projectAt120Bpm();
    project.timing.bpmEvents.push_back(BpmEvent{480, 60.0});
    const PlacementId placement = project.addPlacement(project.addAsset("a.wav"), 240, 0);

    EXPECT_EQ(sampleOffsetInPlacement(project, placement, 720, kRate), 1500);
    EXPECT_EQ(sampleOffsetInPlacement(project, placement, 0, kRate), 0);
}

TEST(PlaybackRange, RNG003_PlacementEndPulseCoversTheWholeAudio) {
    Project project = projectAt120Bpm();
    const PlacementId placement = project.addPlacement(project.addAsset("a.wav"), 960, 0);

    EXPECT_EQ(placementEndPulse(project, placement, 1.5), 1680);
}

TEST(PlaybackRange, RNG004_MixPlacesEachSourceAtItsPlacementTime) {
    Project project = projectAt120Bpm();
    const AssetId asset = project.addAsset("a.wav");
    const PlacementId early = project.addPlacement(asset, 0, 0);
    const PlacementId late = project.addPlacement(asset, 240, 0);
    const AudioBuffer audio = monoBuffer(std::vector<float>(1000, 0.25f));

    const AudioBuffer mix = renderMix(project, {{early, &audio, 1.0f}, {late, &audio, 2.0f}}, 0.25, 1.0, kRate);

    ASSERT_EQ(mix.channels, 2);
    ASSERT_EQ(mix.frameCount(), 750u);
    EXPECT_FLOAT_EQ(mix.interleavedSamples[0], 0.25f);
    EXPECT_FLOAT_EQ(mix.interleavedSamples[1], 0.25f);
    EXPECT_FLOAT_EQ(mix.interleavedSamples[2 * 300], 0.75f);
}

TEST(PlaybackRange, RNG005_MixResamplesWithLinearInterpolation) {
    Project project = projectAt120Bpm();
    const PlacementId placement = project.addPlacement(project.addAsset("a.wav"), 0, 0);
    const AudioBuffer audio = monoBuffer({0.0f, 1.0f, 0.0f, 1.0f, 0.0f}, 500);

    const AudioBuffer mix = renderMix(project, {{placement, &audio, 1.0f}}, 0.0, 0.006, kRate);

    ASSERT_EQ(mix.frameCount(), 6u);
    EXPECT_FLOAT_EQ(mix.interleavedSamples[0], 0.0f);
    EXPECT_FLOAT_EQ(mix.interleavedSamples[2], 0.5f);
    EXPECT_FLOAT_EQ(mix.interleavedSamples[4], 1.0f);
    EXPECT_THROW(static_cast<void>(renderMix(project, {}, 0.0, 1.0, 0)), std::invalid_argument);
}

TEST(PlaybackRange, RNG006_RenderRangeExtractsOneSliceAsStereo) {
    const AudioBuffer audio{kRate, 2, {0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f, 0.4f, -0.4f}};

    const AudioBuffer same = renderRange(audio, SampleRange{1, 3}, kRate);
    const AudioBuffer doubled = renderRange(audio, SampleRange{1, 3}, 2 * kRate);
    const AudioBuffer outside = renderRange(audio, SampleRange{3, 10}, kRate);

    EXPECT_EQ(same.interleavedSamples, (std::vector<float>{0.2f, -0.2f, 0.3f, -0.3f}));
    ASSERT_EQ(doubled.frameCount(), 4u);
    EXPECT_FLOAT_EQ(doubled.interleavedSamples[2], 0.25f);
    EXPECT_FLOAT_EQ(doubled.interleavedSamples[6], 0.3f);
    EXPECT_EQ(outside.frameCount(), 1u);
}

}  // namespace
}  // namespace ibs::core
