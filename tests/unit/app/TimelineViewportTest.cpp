#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/app/TimelineViewport.hpp"

namespace ibs::app {
namespace {

TEST(TimelineViewport, VPT001_ZoomKeepsThePulseUnderTheCursor) {
    TimelineViewport viewport(4.0);
    viewport.setOrigin(960.0);
    EXPECT_DOUBLE_EQ(viewport.pulseAt(viewport.xOf(1234.0)), 1234.0);

    const double anchor = viewport.pulseAt(300.0);
    viewport.zoomAround(300.0, 2.0);

    EXPECT_DOUBLE_EQ(viewport.pulsesPerPixel(), 2.0);
    EXPECT_DOUBLE_EQ(viewport.pulseAt(300.0), anchor);
}

TEST(TimelineViewport, VPT002_ClampsZoomAndNeverScrollsBeforeTheSongStart) {
    TimelineViewport viewport(4.0, 1.0, 8.0);

    viewport.zoomAround(0.0, 100.0);
    EXPECT_DOUBLE_EQ(viewport.pulsesPerPixel(), 1.0);
    viewport.zoomAround(0.0, 0.001);
    EXPECT_DOUBLE_EQ(viewport.pulsesPerPixel(), 8.0);
    viewport.scrollByPixels(-1000.0);
    EXPECT_DOUBLE_EQ(viewport.originPulse(), 0.0);
    EXPECT_THROW(TimelineViewport(1.0, 0.0, 1.0), std::invalid_argument);
}

TEST(TimelineViewport, VPT003_ThinsOutGridLinesThatWouldBeTooDense) {
    TimelineViewport viewport(4.0);

    EXPECT_EQ(viewport.gridDivisionsFor(240, 8.0, 16), 16);
    viewport.setPulsesPerPixel(16.0);
    EXPECT_EQ(viewport.gridDivisionsFor(240, 8.0, 16), 4);
    EXPECT_EQ(viewport.gridDivisionsFor(240, 8.0, 12), 6);
    viewport.setPulsesPerPixel(200.0);
    EXPECT_EQ(viewport.gridDivisionsFor(240, 8.0, 16), 1);
}

TEST(TimelineViewport, VPT004_SnapsToGridFineGridOrNothing) {
    core::TimingData timing;
    timing.resolution = 240;

    EXPECT_EQ(snapPulse(timing, 130.0, 16, SnapMode::Grid), 120);
    EXPECT_EQ(snapPulse(timing, 130.0, 16, SnapMode::Fine), 135);
    EXPECT_EQ(snapPulse(timing, 130.4, 16, SnapMode::Off), 130);
    EXPECT_EQ(snapPulse(timing, -50.0, 16, SnapMode::Grid), 0);
}

TEST(TimelineViewport, VPT005_FitShowsTheWholeRange) {
    TimelineViewport viewport(4.0);

    viewport.fit(960.0, 2880.0, 960.0);

    EXPECT_DOUBLE_EQ(viewport.originPulse(), 960.0);
    EXPECT_DOUBLE_EQ(viewport.pulsesPerPixel(), 2.0);
    EXPECT_DOUBLE_EQ(viewport.xOf(2880.0), 960.0);
}

}  // namespace
}  // namespace ibs::app
