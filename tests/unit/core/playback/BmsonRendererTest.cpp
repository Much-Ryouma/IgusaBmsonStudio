#include <cmath>

#include <gtest/gtest.h>

#include "../../support/SyntheticAudio.hpp"
#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/playback/BmsonRenderer.hpp"
#include "ibs/core/playback/PlaybackRange.hpp"

namespace ibs::core {
namespace {

constexpr int kRate = 8000;

AudioBuffer noisePhrase(double seconds, std::uint32_t seed) {
    std::vector<float> samples(static_cast<std::size_t>(seconds * kRate));
    ibs::testing::DeterministicNoise noise(seed);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = 0.3f * noise.next() * static_cast<float>(0.5 + 0.5 * std::sin(static_cast<double>(index) / 400.0));
    }
    return AudioBuffer{kRate, 1, std::move(samples)};
}

struct Song {
    Project project;
    AudioBuffer bass = noisePhrase(2.0, 1);
    AudioBuffer lead = noisePhrase(1.5, 2);
    PlacementId bassFirst;
    PlacementId bassOverlap;
    PlacementId leadPlacement;

    Song() {
        project.timing.initialBpm = 120.0;
        project.timing.resolution = 240;
        const auto bassAsset = project.addAsset("bass.wav");
        const auto leadAsset = project.addAsset("lead.wav");
        bassFirst = project.addPlacement(bassAsset, 0, 0);
        bassOverlap = project.addPlacement(bassAsset, 960, 0);
        leadPlacement = project.addPlacement(leadAsset, 480, 0);
        project.addNote(project.addCutPoint(bassFirst, 240, CutOrigin::Auto), 1, 0);
        project.addCutPoint(bassFirst, 600, CutOrigin::Auto);
        project.addNote(project.addCutPoint(bassOverlap, 120, CutOrigin::Auto), 3, 240);
        project.addNote(project.headCutPointOf(leadPlacement), 5, 0);
        project.addCutPoint(leadPlacement, 360, CutOrigin::Manual);
    }

    AudioBuffer reference() const {
        std::vector<MixSource> sources{{bassFirst, &bass, 1.0f}, {bassOverlap, &bass, 1.0f}, {leadPlacement, &lead, 1.0f}};
        return renderMix(project, sources, 0.0, 6.0, kRate);
    }

    AudioBuffer exported() const {
        const auto document = bmson::exportDocument(project);
        return renderBmson(document, {{"bass.wav", &bass}, {"lead.wav", &lead}}, kRate);
    }
};

TEST(BmsonRenderer, NUL001_ExportedChartSoundsLikeTheSimpleMix) {
    const Song song;

    const auto comparison = compareRenders(song.reference(), song.exported());

    EXPECT_NEAR(comparison.offsetSeconds, 0.0, 1e-9);
    EXPECT_NEAR(comparison.gain, 1.0, 1e-3);
    EXPECT_LT(comparison.residualDb, -60.0);
}

TEST(BmsonRenderer, NUL002_OneSliceOnTwoLanesIsHeardTwice) {
    Song song;
    const auto layered = song.project.cutPointsOf(song.bassFirst)[1];
    song.project.addNote(layered, 2, 0);

    const auto comparison = compareRenders(song.reference(), song.exported());

    EXPECT_GT(comparison.residualDb, -20.0);
    const auto worst = std::max_element(comparison.windows.begin(), comparison.windows.end(),
                                        [](const auto& a, const auto& b) { return a.residualDb < b.residualDb; });
    EXPECT_NEAR(worst->startSeconds, 0.5, 0.26);
}

TEST(BmsonRenderer, NUL003_ComparisonFindsDelayAndVolume) {
    const Song song;
    const auto reference = song.reference();
    AudioBuffer delayed = reference;
    const auto shift = static_cast<std::size_t>(0.15 * kRate) * 2;
    delayed.interleavedSamples.insert(delayed.interleavedSamples.begin(), shift, 0.0f);
    for (auto& sample : delayed.interleavedSamples) {
        sample *= 0.5f;
    }

    const auto comparison = compareRenders(reference, delayed);

    EXPECT_NEAR(comparison.offsetSeconds, 0.15, 1.0 / kRate);
    EXPECT_NEAR(comparison.gain, 2.0, 1e-3);
    EXPECT_LT(comparison.residualDb, -60.0);
}

TEST(BmsonRenderer, NUL006_LongRenderWithLateStartIsAlignedFromItsLoudestPart) {
    AudioBuffer reference{kRate, 1, std::vector<float>(static_cast<std::size_t>(30.0 * kRate), 0.0f)};
    const auto loud = noisePhrase(4.0, 7);
    const auto quiet = noisePhrase(30.0, 8);
    for (std::size_t index = 0; index < reference.interleavedSamples.size(); ++index) {
        reference.interleavedSamples[index] = 0.05f * quiet.interleavedSamples[index];
    }
    const auto loudStart = static_cast<std::size_t>(20.0 * kRate);
    for (std::size_t index = 0; index < loud.interleavedSamples.size(); ++index) {
        reference.interleavedSamples[loudStart + index] += loud.interleavedSamples[index];
    }
    AudioBuffer delayed = reference;
    delayed.interleavedSamples.insert(delayed.interleavedSamples.begin(), static_cast<std::size_t>(1.9 * kRate), 0.0f);

    const auto comparison = compareRenders(reference, delayed);

    EXPECT_NEAR(comparison.offsetSeconds, 1.9, 1.0 / kRate);
    EXPECT_LT(comparison.residualDb, -60.0);
}

TEST(BmsonRenderer, NUL004_StopMovesTheMixAndTheChartAlike) {
    Song song;
    song.project.addCutPoint(song.bassFirst, 480, CutOrigin::Auto);
    song.project.timing.stopEvents.push_back(StopEvent{360, 120});

    const auto comparison = compareRenders(song.reference(), song.exported());

    EXPECT_LT(comparison.residualDb, -60.0);
}

TEST(BmsonRenderer, NUL005_RejectsMismatchedSampleRates) {
    const AudioBuffer a{8000, 1, std::vector<float>(10, 0.0f)};
    const AudioBuffer b{16000, 1, std::vector<float>(10, 0.0f)};

    EXPECT_THROW(static_cast<void>(compareRenders(a, b)), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
