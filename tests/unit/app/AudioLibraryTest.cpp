#include <gtest/gtest.h>

#include "../support/FakeAppPorts.hpp"
#include "ibs/app/AudioLibrary.hpp"

namespace ibs::app {
namespace {

using ibs::testing::FilesOnlyStorage;
using ibs::testing::InMemoryAudioSource;

const std::filesystem::path kChart = "chart";

core::AudioBuffer stereoOfLength(std::size_t frames) {
    return core::AudioBuffer{1000, 2, std::vector<float>(frames * 2, 0.5f)};
}

TEST(AudioLibrary, AUL001_LoadsOnceThroughTheExtensionFallback) {
    FilesOnlyStorage storage({"bass.ogg"});
    InMemoryAudioSource source;
    source.buffers[kChart / "bass.ogg"] = stereoOfLength(500);
    AudioLibrary library(storage, source);
    library.setChartDirectory(kChart);

    const auto first = library.get("bass.wav");
    const auto second = library.get("bass.wav");

    ASSERT_NE(first.audio, nullptr);
    EXPECT_EQ(first.audio, second.audio);
    EXPECT_EQ(first.audio->file, kChart / "bass.ogg");
    EXPECT_EQ(first.audio->mono.size(), 500u);
    EXPECT_EQ(first.audio->waveform.sampleCount(), 500u);
    EXPECT_EQ(source.loaded.size(), 1u);
}

TEST(AudioLibrary, AUL002_RemembersFailuresUntilInvalidated) {
    FilesOnlyStorage storage;
    InMemoryAudioSource source;
    AudioLibrary library(storage, source);
    library.setChartDirectory(kChart);

    const auto missing = library.get("lead.wav");
    static_cast<void>(library.get("lead.wav"));
    EXPECT_EQ(missing.audio, nullptr);
    EXPECT_NE(missing.error.find("lead.wav"), std::string::npos);
    EXPECT_EQ(storage.lookups, 1);

    library.invalidate("lead.wav");
    static_cast<void>(library.get("lead.wav"));
    EXPECT_EQ(storage.lookups, 2);
}

TEST(AudioLibrary, AUL003_ReusesAnalysisUntilItsFrameSettingsChange) {
    FilesOnlyStorage storage({"kick.wav"});
    InMemoryAudioSource source;
    source.buffers[kChart / "kick.wav"] = stereoOfLength(4000);
    AudioLibrary library(storage, source);
    library.setChartDirectory(kChart);
    core::OnsetParameters parameters;

    const auto first = library.analysis("kick.wav", parameters);
    parameters.sensitivity = 0.9;
    const auto sameFrames = library.analysis("kick.wav", parameters);
    parameters.function = core::OnsetFunction::Energy;
    const auto otherFunction = library.analysis("kick.wav", parameters);

    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, sameFrames);
    EXPECT_NE(first, otherFunction);
    EXPECT_EQ(otherFunction->function, core::OnsetFunction::Energy);
}

TEST(AudioLibrary, AUL004_ChangingTheChartFolderDropsTheCache) {
    FilesOnlyStorage storage({"kick.wav"});
    InMemoryAudioSource source;
    source.buffers[kChart / "kick.wav"] = stereoOfLength(10);
    source.buffers[std::filesystem::path("other") / "kick.wav"] = stereoOfLength(20);
    AudioLibrary library(storage, source);
    library.setChartDirectory(kChart);
    static_cast<void>(library.get("kick.wav"));

    library.setChartDirectory("other");

    EXPECT_EQ(library.cached("kick.wav"), nullptr);
    EXPECT_EQ(library.get("kick.wav").audio->mono.size(), 20u);
}

}  // namespace
}  // namespace ibs::app
