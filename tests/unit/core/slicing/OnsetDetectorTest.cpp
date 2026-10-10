#include <cstdlib>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "../../support/SyntheticAudio.hpp"
#include "ibs/core/slicing/OnsetDetector.hpp"

namespace ibs::core {
namespace {

using ibs::testing::addBackgroundNoise;
using ibs::testing::addNoiseBurst;
using ibs::testing::kTestSampleRate;
using ibs::testing::samplesAt;

std::vector<SampleIndex> positionsOf(const std::vector<Onset>& onsets) {
    std::vector<SampleIndex> positions;
    for (const auto& onset : onsets) {
        positions.push_back(onset.sample);
    }
    return positions;
}

void expectMatches(const std::vector<Onset>& onsets, const std::vector<double>& expectedSeconds, double toleranceMs) {
    ASSERT_EQ(onsets.size(), expectedSeconds.size()) << ::testing::PrintToString(positionsOf(onsets));
    for (std::size_t index = 0; index < onsets.size(); ++index) {
        const double actualSeconds = static_cast<double>(onsets[index].sample) / kTestSampleRate;
        EXPECT_NEAR(actualSeconds * 1000.0, expectedSeconds[index] * 1000.0, toleranceMs) << "onset " << index;
    }
}

TEST(OnsetDetector, ONS001_FindsEveryPercussiveHitWithinThreeMilliseconds) {
    std::vector<float> signal(samplesAt(2.5), 0.0f);
    std::vector<double> expected;
    for (int hit = 0; hit < 8; ++hit) {
        const double seconds = 0.25 * (hit + 1);
        addNoiseBurst(signal, samplesAt(seconds), 0.5, 40.0, static_cast<std::uint32_t>(hit + 1));
        expected.push_back(seconds);
    }
    addBackgroundNoise(signal, 0.001);

    expectMatches(detectOnsets(signal, kTestSampleRate, OnsetParameters{}), expected, 3.0);
}

TEST(OnsetDetector, ONS002_FindsPitchChangesInLegatoNotes) {
    std::vector<float> signal(samplesAt(2.2), 0.0f);
    ibs::testing::addLegatoPhrase(signal, samplesAt(0.1), samplesAt(0.3), {220.0, 330.0, 262.0, 392.0, 294.0, 440.0},
                                  0.4);
    const std::vector<double> expected{0.1, 0.4, 0.7, 1.0, 1.3, 1.6};

    expectMatches(detectOnsets(signal, kTestSampleRate, OnsetParameters{}), expected, 10.0);
}

TEST(OnsetDetector, ONS003_FindsNothingInSilence) {
    const std::vector<float> silence(samplesAt(1.0), 0.0f);

    EXPECT_TRUE(detectOnsets(silence, kTestSampleRate, OnsetParameters{}).empty());
}

TEST(OnsetDetector, ONS004_HigherSensitivityNeverFindsFewer) {
    std::vector<float> signal(samplesAt(2.2), 0.0f);
    const double amplitudes[] = {0.5, 0.05, 0.3, 0.02, 0.4, 0.1};
    for (int hit = 0; hit < 6; ++hit) {
        addNoiseBurst(signal, samplesAt(0.2 + 0.3 * hit), amplitudes[hit], 40.0, static_cast<std::uint32_t>(hit + 7));
    }
    addBackgroundNoise(signal, 0.002);

    std::size_t previous = 0;
    for (const double sensitivity : {0.2, 0.5, 0.9}) {
        OnsetParameters parameters;
        parameters.sensitivity = sensitivity;
        const auto count = detectOnsets(signal, kTestSampleRate, parameters).size();
        EXPECT_GE(count, previous) << "sensitivity " << sensitivity;
        previous = count;
    }
}

TEST(OnsetDetector, ONS005_MergesHitsCloserThanMinimumInterval) {
    std::vector<float> signal(samplesAt(1.0), 0.0f);
    addNoiseBurst(signal, samplesAt(0.50), 0.5, 40.0, 3);
    addNoiseBurst(signal, samplesAt(0.51), 0.5, 40.0, 4);
    OnsetParameters parameters;
    parameters.minimumIntervalMs = 30.0;

    EXPECT_EQ(detectOnsets(signal, kTestSampleRate, parameters).size(), 1u);
}

TEST(OnsetDetector, ONS006_IgnoresHitsBelowSilenceGate) {
    std::vector<float> signal(samplesAt(1.2), 0.0f);
    for (int hit = 0; hit < 4; ++hit) {
        addNoiseBurst(signal, samplesAt(0.2 + 0.25 * hit), 0.0003, 40.0, static_cast<std::uint32_t>(hit + 11));
    }
    OnsetParameters parameters;
    parameters.silenceGateDb = -60.0;

    EXPECT_TRUE(detectOnsets(signal, kTestSampleRate, parameters).empty());
}

TEST(OnsetDetector, ONS007_PrerollMovesCutsEarlier) {
    std::vector<float> signal(samplesAt(1.0), 0.0f);
    addNoiseBurst(signal, samplesAt(0.5), 0.5, 40.0, 5);
    OnsetParameters withPreroll;
    withPreroll.prerollMs = 5.0;

    const auto plain = detectOnsets(signal, kTestSampleRate, OnsetParameters{});
    const auto shifted = detectOnsets(signal, kTestSampleRate, withPreroll);

    ASSERT_EQ(plain.size(), 1u);
    ASSERT_EQ(shifted.size(), 1u);
    EXPECT_NEAR(static_cast<double>(plain[0].sample - shifted[0].sample), 0.005 * kTestSampleRate,
                0.0005 * kTestSampleRate);
}

TEST(OnsetDetector, ONS008_RejectsInvalidParameters) {
    const std::vector<float> signal(1024, 0.0f);
    OnsetParameters badSensitivity;
    badSensitivity.sensitivity = 1.5;
    OnsetParameters badFrame;
    badFrame.frameSize = 1000;

    EXPECT_THROW(static_cast<void>(detectOnsets(signal, kTestSampleRate, badSensitivity)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(detectOnsets(signal, kTestSampleRate, badFrame)), std::invalid_argument);
}

TEST(OnsetDetector, ONS009_PickingFromOneAnalysisEqualsFullDetection) {
    std::vector<float> signal(samplesAt(2.2), 0.0f);
    const double amplitudes[] = {0.5, 0.05, 0.3, 0.02, 0.4, 0.1};
    for (int hit = 0; hit < 6; ++hit) {
        addNoiseBurst(signal, samplesAt(0.2 + 0.3 * hit), amplitudes[hit], 40.0, static_cast<std::uint32_t>(hit + 7));
    }
    addBackgroundNoise(signal, 0.002);
    OnsetParameters parameters;
    const OnsetAnalysis analysis = analyzeOnsets(signal, kTestSampleRate, parameters);

    for (const double sensitivity : {0.2, 0.5, 0.9}) {
        parameters.sensitivity = sensitivity;
        parameters.minimumIntervalMs = 60.0 * sensitivity;
        parameters.prerollMs = 2.0;
        EXPECT_EQ(positionsOf(pickOnsets(analysis, parameters)),
                  positionsOf(detectOnsets(signal, kTestSampleRate, parameters)))
            << "sensitivity " << sensitivity;
    }
}

TEST(OnsetDetector, ONS010_RejectsPickingWithMismatchedAnalysis) {
    const std::vector<float> signal(samplesAt(0.5), 0.0f);
    OnsetParameters parameters;
    const OnsetAnalysis analysis = analyzeOnsets(signal, kTestSampleRate, parameters);

    parameters.function = OnsetFunction::Energy;
    EXPECT_FALSE(analysisMatches(analysis, parameters));
    EXPECT_THROW(static_cast<void>(pickOnsets(analysis, parameters)), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
