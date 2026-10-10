#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/core/timing/TimingMap.hpp"

namespace ibs::core {
namespace {

constexpr double kTolerance = 1e-9;

TEST(TimingMap, TIM001_ConvertsPulseToSecondsAtConstantTempo) {
    const TimingMap timing(120.0, 240);

    EXPECT_NEAR(timing.secondsAt(240), 0.5, kTolerance);
}

TEST(TimingMap, TIM002_AppliesTempoChangeFromItsPulseOnward) {
    const TimingMap timing(120.0, 240, {{480, 60.0}});

    EXPECT_NEAR(timing.secondsAt(720), 2.0, kTolerance);
}

TEST(TimingMap, TIM003_NoteOnStopPulseActivatesAtStopStart) {
    const TimingMap timing(60.0, 240, {}, {{240, 240}});

    EXPECT_NEAR(timing.secondsAt(240), 1.0, kTolerance);
}

TEST(TimingMap, TIM004_PulseAfterStopIncludesStopDuration) {
    const TimingMap timing(60.0, 240, {}, {{240, 240}});

    EXPECT_NEAR(timing.secondsAt(241), 2.0 + 1.0 / 240.0, kTolerance);
}

TEST(TimingMap, TIM005_LastTempoWinsWhenSeveralShareAPulse) {
    const TimingMap timing(60.0, 240, {{240, 100.0}, {240, 120.0}});

    EXPECT_DOUBLE_EQ(timing.bpmAt(240), 120.0);
}

TEST(TimingMap, TIM006_StopsOnSamePulseAddUp) {
    const TimingMap timing(60.0, 240, {}, {{240, 240}, {240, 960}});

    EXPECT_NEAR(timing.secondsAt(480), 7.0, kTolerance);
}

TEST(TimingMap, TIM007_StopUsesTempoChangedOnSamePulse) {
    const TimingMap timing(60.0, 240, {{240, 120.0}}, {{240, 240}});

    EXPECT_NEAR(timing.secondsAt(480), 2.0, kTolerance);
}

TEST(TimingMap, TIM008_HonorsNonDefaultResolution) {
    const TimingMap timing(100.0, 96);

    EXPECT_NEAR(timing.secondsAt(384), 2.4, kTolerance);
}

TEST(TimingMap, TIM009_ResultDoesNotDependOnEventOrder) {
    const TimingMap ascending(120.0, 240, {{240, 60.0}, {960, 180.0}}, {{480, 120}});
    const TimingMap descending(120.0, 240, {{960, 180.0}, {240, 60.0}}, {{480, 120}});

    for (const Pulse y : {Pulse{0}, Pulse{240}, Pulse{480}, Pulse{481}, Pulse{960}, Pulse{1500}}) {
        EXPECT_NEAR(ascending.secondsAt(y), descending.secondsAt(y), kTolerance) << "y=" << y;
    }
}

TEST(TimingMap, TIM010_RejectsNonPositiveInitialTempoOrResolution) {
    EXPECT_THROW(TimingMap(0.0, 240), std::invalid_argument);
    EXPECT_THROW(TimingMap(120.0, 0), std::invalid_argument);
}

TEST(TimingMap, TIM011_RejectsNegativeTempoOrNegativePulse) {
    EXPECT_THROW(TimingMap(120.0, 240, {{240, -60.0}}), std::invalid_argument);
    EXPECT_THROW(TimingMap(120.0, 240, {{-1, 60.0}}), std::invalid_argument);
    EXPECT_THROW(TimingMap(120.0, 240, {}, {{-1, 240}}), std::invalid_argument);
}

TEST(TimingMap, TIM012_PulseAtInvertsSecondsAtAcrossTempoChange) {
    const TimingMap timing(120.0, 240, {{480, 60.0}});

    EXPECT_NEAR(timing.pulseAt(2.0), 720.0, kTolerance);
}

TEST(TimingMap, TIM013_TimeDuringStopMapsToStopPulse) {
    const TimingMap timing(60.0, 240, {}, {{240, 240}});

    for (const double seconds : {1.0, 1.5, 2.0}) {
        EXPECT_NEAR(timing.pulseAt(seconds), 240.0, kTolerance) << seconds;
    }
}

TEST(TimingMap, TIM014_TimeAfterStopResumesFromStopPulse) {
    const TimingMap timing(60.0, 240, {}, {{240, 240}});

    EXPECT_NEAR(timing.pulseAt(2.5), 360.0, kTolerance);
}

}  // namespace
}  // namespace ibs::core
