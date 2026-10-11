#include <cmath>

#include <gtest/gtest.h>

#include "../../support/Beat7k.hpp"
#include "ibs/app/ChartStatisticsService.hpp"

namespace ibs::core {
namespace {

TEST(ChartStatistics, STA001_CountsNoteKindsOncePerLaneAndPosition) {
    Project project;
    project.timing.resolution = 240;
    const KeyMode mode = ibs::testing::beat7k();
    const auto bass = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    const auto lead = project.addPlacement(project.addAsset("lead.wav"), 0, 0);
    project.addNote(project.addCutPoint(bass, 240, CutOrigin::Auto), 1, 0);
    project.addNote(project.addCutPoint(lead, 240, CutOrigin::Auto), 1, 0);
    project.addNote(project.addCutPoint(bass, 480, CutOrigin::Auto), 2, 240);
    project.addNote(project.addCutPoint(bass, 720, CutOrigin::Auto), 8, 0);
    project.addNote(project.addCutPoint(bass, 960, CutOrigin::Auto), 8, 120);
    project.addNote(project.addCutPoint(bass, 1200, CutOrigin::Auto), 12, 0);

    const auto notes = playableNoteStatistics(project, mode);
    const auto bgm = bgmStatistics(project, mode);

    EXPECT_EQ(notes.total, 4u);
    EXPECT_EQ(notes.normal, 1u);
    EXPECT_EQ(notes.longNotes, 1u);
    EXPECT_EQ(notes.scratch, 1u);
    EXPECT_EQ(notes.scratchLong, 1u);
    EXPECT_EQ(notes.perLane.at(8), 2u);
    EXPECT_EQ(bgm.slices, 3u);
    EXPECT_EQ(bgm.bgmColumnNotes, 1u);
}

TEST(ChartStatistics, STA002_FindsTheBusiestSecondAndTheAverage) {
    const auto density = densityOf({0.0, 2.0, 2.2, 2.4, 2.9, 3.0, 10.0});

    EXPECT_EQ(density.maxPerSecond, 4u);
    EXPECT_DOUBLE_EQ(density.maxAtSeconds, 2.0);
    EXPECT_DOUBLE_EQ(density.averagePerSecond, 0.7);
    EXPECT_EQ(densityOf({}).maxPerSecond, 0u);
}

TEST(ChartStatistics, STA003_MeasuresAverageAndPeakLevelsAndWhere) {
    AudioBuffer audio{1000, 1, std::vector<float>(3000, 0.1f)};
    for (std::size_t index = 1500; index < 2500; ++index) {
        audio.interleavedSamples[index] = 0.5f;
    }
    audio.interleavedSamples[2000] = -1.0f;

    const auto level = levelOf(audio);

    EXPECT_DOUBLE_EQ(level.durationSeconds, 3.0);
    EXPECT_NEAR(level.peakDb, 0.0, 1e-9);
    EXPECT_DOUBLE_EQ(level.peakAtSeconds, 2.0);
    EXPECT_NEAR(level.loudestSecondAtSeconds, 1.5, 0.002);
    EXPECT_NEAR(level.loudestSecondDb, 20.0 * std::log10(0.5), 0.1);
    EXPECT_LT(level.averageDb, level.loudestSecondDb);
}

TEST(ChartStatistics, STA004_SplitsTheSoundIntoChartAndBgmParts) {
    Project project;
    project.timing.initialBpm = 120.0;
    project.timing.resolution = 240;
    const KeyMode mode = ibs::testing::beat7k();
    const auto placement = project.addPlacement(project.addAsset("tone.wav"), 0, 0);
    project.addNote(project.addCutPoint(placement, 480, CutOrigin::Auto), 3, 0);
    AudioBuffer tone{1000, 1, std::vector<float>(2000, 0.25f)};

    const auto report = app::computeChartStatistics(project, mode, {{"tone.wav", &tone}}, 1000);

    EXPECT_NEAR(report.chartLevel.durationSeconds, 2.0, 0.01);
    EXPECT_NEAR(report.bgmLevel.loudestSecondAtSeconds, 0.0, 0.01);
    EXPECT_NEAR(report.totalLevel.averageDb, 20.0 * std::log10(0.25), 0.1);
    EXPECT_NEAR(report.chartLevel.peakAtSeconds, 1.0, 0.01);
    EXPECT_EQ(report.totalDensity.events, 2u);
    EXPECT_TRUE(report.missingAssets.empty());
}

}  // namespace
}  // namespace ibs::core
