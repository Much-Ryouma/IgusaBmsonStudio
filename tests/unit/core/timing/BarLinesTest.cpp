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

}  // namespace
}  // namespace ibs::core
