#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include <gtest/gtest.h>

#include "../../support/SyntheticAudio.hpp"
#include "ibs/infra/audio/AudioFileDecoder.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::infra {
namespace {

class AudioFileDecoderTest : public ::testing::Test {
protected:
    std::filesystem::path root;

    void SetUp() override {
        std::random_device device;
        root = std::filesystem::temp_directory_path() / pathFromUtf8("ibs_audio_" + std::to_string(device()));
        std::filesystem::create_directories(root);
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

TEST_F(AudioFileDecoderTest, DEC001_DecodesPcm16StereoWav) {
    const std::vector<float> samples{0.5f, -0.5f, 0.25f, 0.0f, -1.0f, 1.0f};
    const auto path = root / "stereo.wav";
    ibs::testing::writePcm16Wav(path, samples, 2);

    const auto buffer = decodeAudioFile(path);

    EXPECT_EQ(buffer.sampleRate, ibs::testing::kTestSampleRate);
    EXPECT_EQ(buffer.channels, 2);
    ASSERT_EQ(buffer.frameCount(), 3u);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        EXPECT_NEAR(buffer.interleavedSamples[index], samples[index], 1.01 / 32767.0) << index;
    }
}

TEST_F(AudioFileDecoderTest, DEC002_OpensJapanesePath) {
    const auto directory = root / pathFromUtf8("パラデータ");
    std::filesystem::create_directories(directory);
    const auto path = directory / pathFromUtf8("ベース.wav");
    ibs::testing::writePcm16Wav(path, std::vector<float>(441, 0.1f), 1);

    EXPECT_EQ(decodeAudioFile(path).frameCount(), 441u);
}

TEST_F(AudioFileDecoderTest, DEC003_MissingFileThrows) {
    EXPECT_THROW(static_cast<void>(decodeAudioFile(root / "missing.wav")), AudioDecodeError);
}

TEST_F(AudioFileDecoderTest, DEC004_NonAudioFileThrows) {
    const auto path = root / "text.wav";
    std::ofstream(path) << "this is not audio";

    EXPECT_THROW(static_cast<void>(decodeAudioFile(path)), AudioDecodeError);
}

}  // namespace
}  // namespace ibs::infra
