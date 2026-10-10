#include <gtest/gtest.h>

#include "ibs/infra/audio/MiniaudioOutput.hpp"

namespace ibs::infra {
namespace {

TEST(MiniaudioOutput, OUT001_PlaysOrReportsWhyWithoutCrashing) {
    MiniaudioOutput output(48000);
    EXPECT_EQ(output.sampleRate(), 48000);

    const bool started = output.start(core::AudioBuffer{48000, 2, std::vector<float>(48000 * 2, 0.0f)});

    if (started) {
        EXPECT_TRUE(output.isPlaying());
        output.stop();
        EXPECT_FALSE(output.isPlaying());
    } else {
        EXPECT_FALSE(output.lastError().empty());
        EXPECT_FALSE(output.isPlaying());
    }
}

TEST(MiniaudioOutput, OUT002_RejectsNonStereoBuffers) {
    MiniaudioOutput output(48000);

    EXPECT_FALSE(output.start(core::AudioBuffer{48000, 1, std::vector<float>(100, 0.0f)}));
    EXPECT_FALSE(output.isPlaying());
}

}  // namespace
}  // namespace ibs::infra
