#include <gtest/gtest.h>

#include "../support/Beat7k.hpp"
#include "../support/FakeAppPorts.hpp"
#include "../support/SyntheticAudio.hpp"
#include "ibs/app/NullTestService.hpp"
#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/playback/BmsonRenderer.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {
namespace {

using ibs::testing::FilesOnlyStorage;
using ibs::testing::InMemoryAudioSource;
using ibs::testing::RecordingRunner;

const std::filesystem::path kChart = "song";
constexpr int kRate = 8000;

core::AudioBuffer phrase(std::uint32_t seed) {
    std::vector<float> samples(2 * kRate);
    ibs::testing::DeterministicNoise noise(seed);
    for (auto& sample : samples) {
        sample = 0.2f * noise.next();
    }
    return core::AudioBuffer{kRate, 1, std::move(samples)};
}

struct Fixture {
    FilesOnlyStorage storage{{"bass.wav", "lead.wav"}};
    InMemoryAudioSource source;
    ChartService charts{storage, [] {
                            core::KeyModeCatalog modes;
                            modes.add(ibs::testing::beat7k());
                            return modes;
                        }()};
    NullTestService service{charts, storage, source};
    core::Project project;
    NullTestOptions options;

    Fixture() {
        source.buffers[kChart / "bass.wav"] = phrase(1);
        source.buffers[kChart / "lead.wav"] = phrase(2);
        project.timing.initialBpm = 120.0;
        project.timing.resolution = 240;
        const auto bass = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
        const auto lead = project.addPlacement(project.addAsset("lead.wav"), 480, 0);
        project.addNote(project.addCutPoint(bass, 240, core::CutOrigin::Auto), 1, 0);
        project.addNote(project.addCutPoint(lead, 240, core::CutOrigin::Auto), 14, 0);
        options.internalSampleRate = kRate;
    }
};

TEST(NullTestService, NTS001_InternalPlayerMatchesTheSimpleMix) {
    Fixture fixture;

    const auto report = fixture.service.compareWithInternalPlayer(fixture.project, kChart, fixture.options);

    ASSERT_TRUE(report.completed) << report.error;
    EXPECT_TRUE(report.passed);
    EXPECT_LT(report.residualDb, -60.0);
    EXPECT_TRUE(report.worst.empty());
}

TEST(NullTestService, NTS002_LayeredSliceIsReportedWithItsBar) {
    Fixture fixture;
    const auto bass = fixture.project.placements().front().id;
    fixture.project.addNote(fixture.project.cutPointsOf(bass)[1], 2, 0);

    const auto report = fixture.service.compareWithInternalPlayer(fixture.project, kChart, fixture.options);

    ASSERT_TRUE(report.completed);
    EXPECT_FALSE(report.passed);
    ASSERT_FALSE(report.worst.empty());
    EXPECT_EQ(report.worst.front().bar, 0);
    EXPECT_GE(report.worst.front().pulse, 0);
}

TEST(NullTestService, NTS003_PlayerRenderingIsComparedAfterFoldingBgmColumns) {
    Fixture fixture;
    RecordingRunner runner;
    auto folded = fixture.project;
    std::map<std::string, const core::AudioBuffer*> audio{{"bass.wav", &fixture.source.buffers[kChart / "bass.wav"]},
                                                          {"lead.wav", &fixture.source.buffers[kChart / "lead.wav"]}};
    auto rendered = core::renderBmson(core::bmson::exportDocument(folded), audio, kRate);
    rendered.interleavedSamples.insert(rendered.interleavedSamples.begin(), 2 * kRate / 10, 0.0f);
    fixture.source.buffers[kChart / ".~nulltest_song.wav"] = rendered;

    const auto report = fixture.service.compareWithPlayer(fixture.project, kChart / "song.bmson", PlayerSettings{"Pulsus.exe"},
                                                          kDefaultRenderTemplate, runner, fixture.options);

    ASSERT_TRUE(report.completed) << report.error;
    EXPECT_NEAR(report.offsetMs, 100.0, 0.2);
    EXPECT_TRUE(report.passed);
    ASSERT_EQ(runner.requests.size(), 1u);
    EXPECT_EQ(runner.requests.front().arguments,
              (std::vector<std::string>{infra::pathToUtf8(kChart / ".~nulltest_song.bmson"), "--render",
                                        infra::pathToUtf8(kChart / ".~nulltest_song.wav")}));
}

TEST(NullTestService, NTS004_ReportsPlayerFailuresAndMissingAssets) {
    Fixture fixture;
    RecordingRunner runner;
    runner.exitCode = 3;
    fixture.project.addPlacement(fixture.project.addAsset("missing.wav"), 0, 0);

    const auto failed = fixture.service.compareWithPlayer(fixture.project, kChart / "song.bmson", PlayerSettings{"Pulsus.exe"},
                                                          kDefaultRenderTemplate, runner, fixture.options);
    const auto internal = fixture.service.compareWithInternalPlayer(fixture.project, kChart, fixture.options);

    EXPECT_FALSE(failed.completed);
    EXPECT_NE(failed.error.find("3"), std::string::npos);
    EXPECT_EQ(internal.missingAssets, std::vector<std::string>{"missing.wav"});
}

TEST(NullTestService, NTS005_RefusesToPassWhenNoAudioCouldBeLoaded) {
    Fixture fixture;
    fixture.source.buffers.clear();
    FilesOnlyStorage empty;
    NullTestService service{fixture.charts, empty, fixture.source};

    const auto report = service.compareWithInternalPlayer(fixture.project, kChart, fixture.options);

    EXPECT_FALSE(report.completed);
    EXPECT_FALSE(report.passed);
    EXPECT_FALSE(report.error.empty());
    EXPECT_EQ(report.missingAssets, (std::vector<std::string>{"bass.wav", "lead.wav"}));
}

}  // namespace
}  // namespace ibs::app
