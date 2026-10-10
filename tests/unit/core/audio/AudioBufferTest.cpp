#include <gtest/gtest.h>

#include "ibs/core/audio/AudioBuffer.hpp"

namespace ibs::core {
namespace {

TEST(AudioBuffer, AUD001_MixesChannelsByAveraging) {
    const AudioBuffer stereo{44100, 2, {1.0f, 0.0f, 0.5f, 0.5f}};

    EXPECT_EQ(mixToMono(stereo), (std::vector<float>{0.5f, 0.5f}));
}

TEST(AudioBuffer, AUD002_ReportsFramesAndDuration) {
    const AudioBuffer mono{44100, 1, std::vector<float>(44100, 0.0f)};

    EXPECT_EQ(mono.frameCount(), 44100u);
    EXPECT_DOUBLE_EQ(mono.durationSeconds(), 1.0);
}

}  // namespace
}  // namespace ibs::core
