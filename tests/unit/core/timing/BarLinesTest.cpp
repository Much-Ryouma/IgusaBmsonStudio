#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/core/timing/BarLines.hpp"

namespace ibs::core {
namespace {

TEST(BarLines, BAR001_DerivesTimeSignatureFromBarLength) {
    EXPECT_EQ(timeSignatureForBarLength(960, 240), (TimeSignature{4, 4}));
    EXPECT_EQ(timeSignatureForBarLength(720, 240), (TimeSignature{3, 4}));
    EXPECT_EQ(timeSignatureForBarLength(840, 240), (TimeSignature{7, 8}));
    EXPECT_EQ(timeSignatureForBarLength(1200, 240), (TimeSignature{5, 4}));
}

TEST(BarLines, BAR002_RejectsLengthThatIsNoTimeSignature) {
    EXPECT_FALSE(timeSignatureForBarLength(1000, 240).has_value());
}

TEST(BarLines, BAR003_AddsImplicitBarAtZero) {
    TimingData timing;
    timing.barLines = std::vector<Pulse>{960, 1920};

    EXPECT_EQ(barStartsOf(timing), (std::vector<Pulse>{0, 960, 1920}));
}

TEST(BarLines, BAR004_GridLinesRestartAtEachBarAndContinueAfterTheLastLine) {
    TimingData timing;
    timing.resolution = 240;
    timing.barLines = std::vector<Pulse>{0, 960, 1800};

    const auto lines = gridLinesBetween(timing, 8, 600, 2400);

    std::vector<Pulse> pulses;
    for (const auto& line : lines) {
        pulses.push_back(line.pulse);
    }
    EXPECT_EQ(pulses, (std::vector<Pulse>{600, 720, 840, 960, 1080, 1200, 1320, 1440, 1560, 1680, 1800, 1920, 2040,
                                          2160, 2280, 2400}));
    EXPECT_EQ(lines[3], (GridLine{960, GridLineKind::Bar, 1}));
    EXPECT_EQ(lines[4].kind, GridLineKind::Subdivision);
    EXPECT_EQ(lines[5].kind, GridLineKind::Beat);
    EXPECT_EQ(lines[10], (GridLine{1800, GridLineKind::Bar, 2}));
    EXPECT_EQ(lines[15], (GridLine{2400, GridLineKind::Subdivision, 2}));
    EXPECT_EQ(gridLinesBetween(timing, 8, 2640, 2640), (std::vector<GridLine>{{2640, GridLineKind::Bar, 3}}));
}

TEST(BarLines, BAR005_GridLinesOfTripletGridMarkBeatsAndRejectZeroDivisions) {
    TimingData timing;
    timing.resolution = 240;

    const auto lines = gridLinesBetween(timing, 12, 0, 959);

    ASSERT_EQ(lines.size(), 12u);
    EXPECT_EQ(lines[1].pulse, 80);
    EXPECT_EQ(lines[3], (GridLine{240, GridLineKind::Beat, 0}));
    EXPECT_THROW(static_cast<void>(gridLinesBetween(timing, 0, 0, 960)), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
