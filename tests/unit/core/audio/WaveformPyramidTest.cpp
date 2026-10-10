#include <vector>

#include <gtest/gtest.h>

#include "ibs/core/audio/WaveformPyramid.hpp"

namespace ibs::core {
namespace {

std::vector<float> rampWithSpike() {
    std::vector<float> samples(4096);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = static_cast<float>(index % 64) / 128.0f - 0.25f;
    }
    samples[3000] = 0.9f;
    samples[3001] = -0.8f;
    return samples;
}

TEST(WaveformPyramid, WAV001_RangeEqualsExactMinMaxOfTheSamples) {
    const auto samples = rampWithSpike();
    const WaveformPyramid pyramid(samples, 16, 4, 4);

    for (const auto& [begin, end] : std::vector<std::pair<SampleIndex, SampleIndex>>{{0, 4096}, {2048, 3072}, {2990, 3010}}) {
        const auto [lowest, highest] = pyramid.range(begin, end);
        float expectedLow = 1.0f;
        float expectedHigh = -1.0f;
        for (SampleIndex index = begin; index < end; ++index) {
            expectedLow = std::min(expectedLow, samples[static_cast<std::size_t>(index)]);
            expectedHigh = std::max(expectedHigh, samples[static_cast<std::size_t>(index)]);
        }
        EXPECT_FLOAT_EQ(lowest, expectedLow) << begin << "-" << end;
        EXPECT_FLOAT_EQ(highest, expectedHigh) << begin << "-" << end;
    }
}

TEST(WaveformPyramid, WAV002_BuildsCoarserLevelsByTheReductionFactor) {
    const WaveformPyramid pyramid(rampWithSpike(), 16, 4, 4);

    ASSERT_EQ(pyramid.levels().size(), 4u);
    EXPECT_EQ(pyramid.levels()[0].samplesPerBin, 16u);
    EXPECT_EQ(pyramid.levels()[3].samplesPerBin, 1024u);
    EXPECT_EQ(pyramid.levels()[3].maximum.size(), 4u);
}

TEST(WaveformPyramid, WAV003_EmptyOrOutOfRangeRequestsReturnZero) {
    const WaveformPyramid empty(std::vector<float>{});
    const WaveformPyramid pyramid(rampWithSpike(), 16, 4, 4);

    EXPECT_EQ(empty.range(0, 100), std::make_pair(0.0f, 0.0f));
    EXPECT_EQ(pyramid.range(5000, 6000), std::make_pair(0.0f, 0.0f));
    EXPECT_EQ(pyramid.range(100, 100), std::make_pair(0.0f, 0.0f));
}

}  // namespace
}  // namespace ibs::core
