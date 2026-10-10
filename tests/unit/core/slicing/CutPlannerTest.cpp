#include <algorithm>
#include <cmath>
#include <set>

#include <gtest/gtest.h>

#include "ibs/core/slicing/CutPlanner.hpp"

namespace ibs::core {
namespace {

constexpr int kRate = 44100;

TimingData timingAt(double bpm, int resolution = 240) {
    TimingData timing;
    timing.initialBpm = bpm;
    timing.resolution = resolution;
    return timing;
}

Onset onsetAt(double seconds) { return Onset{static_cast<SampleIndex>(std::llround(seconds * kRate)), 1.0}; }

TEST(BeatGrid, QNT001_CountsGridFromEachBarStart) {
    auto timing = timingAt(120.0);
    timing.barLines = std::vector<Pulse>{0, 960, 1800};
    const BeatGrid tripletEighths(timing, 12);

    EXPECT_DOUBLE_EQ(tripletEighths.nearestGridPulse(1830.0), 1800.0);
}

TEST(BeatGrid, QNT002_ExtendsLastBarLengthBeyondFinalLine) {
    auto timing = timingAt(120.0);
    timing.barLines = std::vector<Pulse>{0, 960};
    const BeatGrid sixteenths(timing, 16);

    EXPECT_DOUBLE_EQ(sixteenths.nearestGridPulse(2165.0), 2160.0);
}

TEST(CutPlanner, CUT001_SnapsSlightlyLateOnsetBackToGrid) {
    const auto plan = planCuts({onsetAt(0.504)}, kRate, 0, timingAt(120.0), QuantizeParameters{});

    ASSERT_EQ(plan.cuts.size(), 1u);
    EXPECT_EQ(plan.cuts[0].offsetPulses, 240);
    EXPECT_TRUE(plan.cuts[0].snapped);
    EXPECT_NEAR(plan.cuts[0].lateMs, -4.0, 0.1);
}

TEST(CutPlanner, CUT002_HalfStrengthMovesHalfway) {
    QuantizeParameters parameters;
    parameters.strength = 0.5;

    const auto plan = planCuts({onsetAt(0.504)}, kRate, 0, timingAt(120.0), parameters);

    ASSERT_EQ(plan.cuts.size(), 1u);
    EXPECT_EQ(plan.cuts[0].offsetPulses, 241);
}

TEST(CutPlanner, CUT003_LeavesDistantOnsetUnsnappedAndNeverAfterAttack) {
    QuantizeParameters parameters;
    parameters.windowMs = 20.0;

    const auto plan = planCuts({onsetAt(0.530)}, kRate, 0, timingAt(120.0), parameters);

    ASSERT_EQ(plan.cuts.size(), 1u);
    EXPECT_FALSE(plan.cuts[0].snapped);
    EXPECT_EQ(plan.cuts[0].offsetPulses, 254);
    EXPECT_LE(plan.cuts[0].lateMs, 0.0);
}

TEST(CutPlanner, CUT004_FlagsSnapThatLandsAfterAttack) {
    Project project;
    project.timing = timingAt(120.0);
    const PlacementId placement = project.addPlacement(project.addAsset("lead.wav"), 0, 0);

    const auto plan = planCuts({onsetAt(0.610)}, kRate, 0, project.timing, QuantizeParameters{});
    const auto issues = attackClipIssues(project, placement, plan);

    ASSERT_EQ(plan.cuts.size(), 1u);
    EXPECT_EQ(plan.cuts[0].offsetPulses, 300);
    EXPECT_TRUE(plan.cuts[0].attackClipped);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].code, "V09");
}

TEST(CutPlanner, CUT005_MergesHeadAndDuplicateCuts) {
    const auto plan =
        planCuts({onsetAt(0.0), onsetAt(0.500), onsetAt(0.505)}, kRate, 0, timingAt(120.0), QuantizeParameters{});

    EXPECT_EQ(plan.mergedIntoHead, 1u);
    EXPECT_EQ(plan.mergedDuplicates, 1u);
    EXPECT_EQ(plan.cuts.size(), 1u);
}

TEST(CutPlanner, CUT006_OffsetsAreRelativeToPlacementStart) {
    QuantizeParameters noGrid;
    noGrid.divisionsPerWholeNote = 0;

    const auto plan = planCuts({onsetAt(0.5)}, kRate, 960, timingAt(60.0), noGrid);

    ASSERT_EQ(plan.cuts.size(), 1u);
    EXPECT_EQ(plan.cuts[0].offsetPulses, 120);
}

TEST(CutPlanner, APL001_ReplacesOnlyUnassignedAutomaticCuts) {
    Project project;
    const PlacementId placement = project.addPlacement(project.addAsset("bass.wav"), 0, 0);
    project.addCutPoint(placement, 120, CutOrigin::Auto);
    project.addNote(project.addCutPoint(placement, 240, CutOrigin::Auto), 1, 0);
    project.addCutPoint(placement, 360, CutOrigin::Manual);
    CutPlan plan;
    plan.cuts = {PlannedCut{240}, PlannedCut{480}};

    const auto applied = applyCutPlan(project, placement, plan);

    EXPECT_EQ(applied.removed, 1u);
    EXPECT_EQ(applied.added, 1u);
    EXPECT_EQ(applied.keptExisting, 1u);
    std::set<Pulse> offsets;
    for (const auto& cutPoint : project.cutPoints()) {
        offsets.insert(cutPoint.offsetPulses);
    }
    EXPECT_EQ(offsets, (std::set<Pulse>{0, 240, 360, 480}));
}

}  // namespace
}  // namespace ibs::core
