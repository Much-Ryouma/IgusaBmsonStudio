#include <memory>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "../support/FakeFileSystem.hpp"
#include "../support/SyntheticAudio.hpp"
#include "ibs/app/SlicingService.hpp"

namespace ibs::app {
namespace {

using ibs::testing::kTestSampleRate;
using ibs::testing::samplesAt;

class NoFilesStorage final : public ChartStorage {
public:
    infra::BmsonParseResult load(const std::filesystem::path&) override { return {}; }
    void save(const std::filesystem::path&, const core::bmson::Document&) override {}
    std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path&) override {
        return std::make_unique<ibs::testing::FakeFileSystem>();
    }
};

class UnusedAudioSource final : public AudioSource {
public:
    core::AudioBuffer load(const std::filesystem::path&) override { return {}; }
};

core::AudioBuffer fourHits() {
    std::vector<float> signal(samplesAt(2.0), 0.0f);
    for (int hit = 0; hit < 4; ++hit) {
        ibs::testing::addNoiseBurst(signal, samplesAt(0.5 * hit), 0.5, 40.0, static_cast<std::uint32_t>(hit + 1));
    }
    return core::AudioBuffer{kTestSampleRate, 1, std::move(signal)};
}

core::Project projectWithPlacement(core::PlacementId& placement) {
    core::Project project;
    project.timing.initialBpm = 120.0;
    project.timing.resolution = 240;
    placement = project.addPlacement(project.addAsset("drums.wav"), 0, 0);
    return project;
}

std::set<core::Pulse> offsetsOf(const core::Project& project) {
    std::set<core::Pulse> offsets;
    for (const auto& cutPoint : project.cutPoints()) {
        offsets.insert(cutPoint.offsetPulses);
    }
    return offsets;
}

TEST(SlicingService, SLC001_AddsAutomaticCutsForDetectedHits) {
    core::PlacementId placement;
    auto project = projectWithPlacement(placement);

    const auto outcome = SlicingService::sliceBuffer(project, placement, fourHits(), SliceSettings{});

    EXPECT_TRUE(outcome.succeeded);
    EXPECT_EQ(outcome.applied.added, 3u);
    EXPECT_EQ(offsetsOf(project), (std::set<core::Pulse>{0, 240, 480, 720}));
}

TEST(SlicingService, SLC002_ReportsMissingAssetByName) {
    core::PlacementId placement;
    auto project = projectWithPlacement(placement);
    NoFilesStorage storage;
    UnusedAudioSource audio;
    const SlicingService service(storage, audio);

    const auto outcome = service.slicePlacement(project, placement, "chart-folder", SliceSettings{});

    EXPECT_FALSE(outcome.succeeded);
    EXPECT_NE(outcome.error.find("drums.wav"), std::string::npos);
}

TEST(SlicingService, SLC003_ResliceKeepsCutsWithAssignedLanes) {
    core::PlacementId placement;
    auto project = projectWithPlacement(placement);
    static_cast<void>(SlicingService::sliceBuffer(project, placement, fourHits(), SliceSettings{}));
    for (const auto& cutPoint : project.cutPoints()) {
        if (cutPoint.offsetPulses == 480) {
            project.addNote(cutPoint.id, 3, 0);
        }
    }
    const core::AudioBuffer silence{kTestSampleRate, 1, std::vector<float>(samplesAt(2.0), 0.0f)};

    static_cast<void>(SlicingService::sliceBuffer(project, placement, silence, SliceSettings{}));

    EXPECT_EQ(offsetsOf(project), (std::set<core::Pulse>{0, 480}));
}

}  // namespace
}  // namespace ibs::app
