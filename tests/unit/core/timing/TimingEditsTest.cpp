#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::core {
namespace {

TimingData timing240() {
    TimingData timing;
    timing.initialBpm = 120.0;
    timing.resolution = 240;
    return timing;
}

TEST(TimingEdits, TED001_BpmChangesAreSortedReplacedAndAtZeroBecomeTheInitialBpm) {
    TimingData timing = timing240();

    setBpmChange(timing, 960, 150.0);
    setBpmChange(timing, 480, 140.0);
    setBpmChange(timing, 960, 160.0);
    setBpmChange(timing, 0, 130.0);

    EXPECT_EQ(timing.bpmEvents, (std::vector<BpmEvent>{{480, 140.0}, {960, 160.0}}));
    EXPECT_DOUBLE_EQ(timing.initialBpm, 130.0);
    EXPECT_THROW(setBpmChange(timing, 240, 0.0), std::invalid_argument);
    removeBpmChange(timing, 480);
    EXPECT_EQ(timing.bpmEvents.size(), 1u);
    EXPECT_THROW(removeBpmChange(timing, 480), std::invalid_argument);
}

TEST(TimingEdits, TED002_StopsAreSortedAndReplaced) {
    TimingData timing = timing240();

    setStop(timing, 960, 120);
    setStop(timing, 480, 60);
    setStop(timing, 960, 240);

    EXPECT_EQ(timing.stopEvents, (std::vector<StopEvent>{{480, 60}, {960, 240}}));
    EXPECT_THROW(setStop(timing, 240, 0), std::invalid_argument);
    removeStop(timing, 480);
    EXPECT_THROW(removeStop(timing, 480), std::invalid_argument);
}

TEST(TimingEdits, TED003_TimeSignatureAppliesUntilTheNextChange) {
    TimingData timing = timing240();

    setTimeSignature(timing, 2, TimeSignature{7, 8}, 4000);
    setTimeSignature(timing, 4, TimeSignature{4, 4}, 4000);

    ASSERT_TRUE(timing.barLines.has_value());
    const std::vector<Pulse> expectedStart{0, 960, 1920, 2760, 3600, 4560};
    EXPECT_EQ(std::vector<Pulse>(timing.barLines->begin(), timing.barLines->begin() + 6), expectedStart);
    EXPECT_EQ(barLengthChangesOf(timing),
              (std::vector<BarLengthChange>{{0, 960}, {2, 840}, {4, 960}}));
    EXPECT_GT(timing.barLines->back(), 4000);
}

TEST(TimingEdits, TED004_RemovingAChangeContinuesThePreviousSignature) {
    TimingData timing = timing240();
    setTimeSignature(timing, 2, TimeSignature{7, 8}, 4000);
    setTimeSignature(timing, 4, TimeSignature{4, 4}, 4000);

    removeTimeSignatureChange(timing, 4, 4000);

    EXPECT_EQ(barLengthChangesOf(timing), (std::vector<BarLengthChange>{{0, 960}, {2, 840}}));
    EXPECT_THROW(removeTimeSignatureChange(timing, 0, 4000), std::invalid_argument);
    EXPECT_THROW(removeTimeSignatureChange(timing, 3, 4000), std::invalid_argument);
}

TEST(TimingEdits, TED005_RejectsSignatureThatDoesNotFitTheResolution) {
    TimingData timing = timing240();
    timing.resolution = 5;

    EXPECT_THROW(setTimeSignature(timing, 0, TimeSignature{7, 8}, 1000), std::invalid_argument);
    EXPECT_EQ(barLengthOf(TimeSignature{5, 4}, 240), 1200);
}

TEST(TimingEdits, TED006_ManualBarLinesKeepTheDefaultBarsBeforeThem) {
    TimingData timing = timing240();

    insertBarLine(timing, 2400, 3000);

    ASSERT_TRUE(timing.barLines.has_value());
    EXPECT_EQ(std::vector<Pulse>(timing.barLines->begin(), timing.barLines->begin() + 5),
              (std::vector<Pulse>{0, 960, 1920, 2400, 2880}));
    EXPECT_THROW(insertBarLine(timing, 960, 3000), std::invalid_argument);
    removeBarLine(timing, 2400);
    EXPECT_THROW(removeBarLine(timing, 2400), std::invalid_argument);
    EXPECT_THROW(removeBarLine(timing, 0), std::invalid_argument);
}

TEST(TimingEdits, TED007_FindsBarIndexAndStartIncludingBeyondTheLastLine) {
    TimingData timing = timing240();
    timing.barLines = std::vector<Pulse>{0, 960, 1800};

    EXPECT_EQ(barIndexAt(timing, 0), 0);
    EXPECT_EQ(barIndexAt(timing, 1799), 1);
    EXPECT_EQ(barIndexAt(timing, 1800), 2);
    EXPECT_EQ(barIndexAt(timing, 2640), 3);
    EXPECT_EQ(barStartPulse(timing, 1), 960);
    EXPECT_EQ(barStartPulse(timing, 3), 2640);
}

}  // namespace
}  // namespace ibs::core
