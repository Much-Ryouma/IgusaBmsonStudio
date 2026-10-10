#include <gtest/gtest.h>

#include "../support/FakeAppPorts.hpp"
#include "../support/SyntheticAudio.hpp"
#include "ibs/app/ArrangementCommands.hpp"
#include "ibs/app/SlicingSession.hpp"

namespace ibs::app {
namespace {

using ibs::testing::FilesOnlyStorage;
using ibs::testing::InMemoryAudioSource;
using ibs::testing::kTestSampleRate;
using ibs::testing::samplesAt;

const std::filesystem::path kChart = "chart";

core::AudioBuffer hitsAt(const std::vector<double>& seconds) {
    std::vector<float> signal(samplesAt(2.0), 0.0f);
    std::uint32_t seed = 1;
    for (const double at : seconds) {
        ibs::testing::addNoiseBurst(signal, samplesAt(at), 0.5, 40.0, seed++);
    }
    return core::AudioBuffer{kTestSampleRate, 1, std::move(signal)};
}

struct Fixture {
    FilesOnlyStorage storage{{"drums.wav"}};
    InMemoryAudioSource source;
    AudioLibrary library{storage, source};
    EditorDocument document;
    SlicingSession session{document, library};
    core::PlacementId placement;
    core::CutPointId staleAuto;

    explicit Fixture(const std::vector<double>& hits = {0.5, 1.0, 1.485}) {
        source.buffers[kChart / "drums.wav"] = hitsAt(hits);
        library.setChartDirectory(kChart);
        core::Project project;
        project.timing.initialBpm = 120.0;
        project.timing.resolution = 240;
        placement = project.addPlacement(project.addAsset("drums.wav"), 0, 0);
        staleAuto = project.addCutPoint(placement, 30, core::CutOrigin::Auto);
        document.replace(std::move(project), kChart / "song.bmson");
    }

    std::set<core::Pulse> offsets() const {
        std::set<core::Pulse> result;
        for (const auto id : document.project().cutPointsOf(placement)) {
            result.insert(document.project().cutPoint(id).offsetPulses);
        }
        return result;
    }
};

TEST(SlicingSession, SLS001_PreviewLeavesTheDocumentUntouched) {
    Fixture fixture;

    const auto preview = fixture.session.preview(fixture.placement);

    ASSERT_TRUE(preview.has_value());
    EXPECT_EQ(preview->detectedOnsets, 3u);
    ASSERT_EQ(preview->plan.cuts.size(), 3u);
    EXPECT_EQ(preview->plan.cuts[0].offsetPulses, 240);
    EXPECT_EQ(preview->replaced, std::vector<core::CutPointId>{fixture.staleAuto});
    EXPECT_FALSE(fixture.document.canUndo());
    EXPECT_EQ(fixture.offsets(), (std::set<core::Pulse>{0, 30}));
}

TEST(SlicingSession, SLS002_ApplyIsASingleUndoStep) {
    Fixture fixture;
    const auto preview = fixture.session.preview(fixture.placement);

    const auto outcome = fixture.session.apply(*preview);

    ASSERT_TRUE(outcome.succeeded) << outcome.error;
    EXPECT_EQ(fixture.offsets(), (std::set<core::Pulse>{0, 240, 480, 720}));
    fixture.document.undo();
    EXPECT_EQ(fixture.offsets(), (std::set<core::Pulse>{0, 30}));
}

TEST(SlicingSession, SLS003_RefusesPreviewMadeBeforeThePlacementMoved) {
    Fixture fixture;
    const auto preview = fixture.session.preview(fixture.placement);
    movePlacement(fixture.document, fixture.placement, 960);

    const auto outcome = fixture.session.apply(*preview);

    EXPECT_FALSE(outcome.succeeded);
    EXPECT_FALSE(outcome.error.empty());
    EXPECT_EQ(fixture.offsets(), (std::set<core::Pulse>{0, 30}));
}

TEST(SlicingSession, SLS004_MarksClippedAttacksUntilTheCutIsMovedByHand) {
    Fixture fixture;
    auto preview = fixture.session.preview(fixture.placement);
    fixture.session.apply(*preview);
    const auto& project = fixture.document.project();
    core::CutPointId clipped;
    core::CutPointId clean;
    for (const auto id : project.cutPointsOf(fixture.placement)) {
        if (project.cutPoint(id).offsetPulses == 720) {
            clipped = id;
        } else if (project.cutPoint(id).offsetPulses == 240) {
            clean = id;
        }
    }

    EXPECT_TRUE(fixture.session.isAttackClipped(clipped));
    EXPECT_FALSE(fixture.session.isAttackClipped(clean));
    moveCut(fixture.document, clipped, 712);
    EXPECT_FALSE(fixture.session.isAttackClipped(clipped));
}

TEST(SlicingSession, SLS005_UsesSettingsPerPlacementAndReportsMissingAudio) {
    Fixture fixture;
    SliceSettings coarse;
    coarse.quantize.divisionsPerWholeNote = 4;
    fixture.session.setSettings(fixture.placement, coarse);
    EXPECT_EQ(fixture.session.settingsFor(fixture.placement).quantize.divisionsPerWholeNote, 4);
    const auto other = fixture.document.apply("配置", [](core::Project& project) {
        return project.addPlacement(project.addAsset("missing.wav"), 0, 0);
    });
    EXPECT_EQ(fixture.session.settingsFor(other).quantize.divisionsPerWholeNote, 16);

    std::string error;
    EXPECT_FALSE(fixture.session.preview(other, &error).has_value());
    EXPECT_NE(error.find("missing.wav"), std::string::npos);
}

}  // namespace
}  // namespace ibs::app
