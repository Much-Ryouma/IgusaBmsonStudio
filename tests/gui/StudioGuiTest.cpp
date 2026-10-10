#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTest>

#include <gtest/gtest.h>

#include "GuiFixture.hpp"
#include "SlicerPanel.hpp"

namespace ibs::gui::testing {
namespace {

std::vector<core::Pulse> cutPulses(const core::Project& project, core::PlacementId placement) {
    std::vector<core::Pulse> pulses;
    for (const auto id : project.cutPointsOf(placement)) {
        pulses.push_back(project.absolutePulseOf(project.cutPoint(id)));
    }
    return pulses;
}

std::optional<core::CutPointId> cutAt(const core::Project& project, core::PlacementId placement, core::Pulse pulse) {
    for (const auto id : project.cutPointsOf(placement)) {
        if (project.absolutePulseOf(project.cutPoint(id)) == pulse) {
            return id;
        }
    }
    return std::nullopt;
}

TEST(StudioGui, GUI001_DoubleClickAddsSnappedManualCut) {
    GuiFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    fixture.settle();

    GuiFixture::doubleClick(fixture.window().slicer(), fixture.slicerPoint(1205.0));

    const auto cut = cutAt(fixture.project(), fixture.placement, 1200);
    ASSERT_TRUE(cut.has_value());
    EXPECT_EQ(fixture.project().cutPoint(*cut).origin, core::CutOrigin::Manual);
    EXPECT_EQ(fixture.controller().selectedCuts(), std::vector<core::CutPointId>{*cut});
}

TEST(StudioGui, GUI002_DraggingCutMovesItWithSnapOrFreelyWithAlt) {
    GuiFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    const auto cut = app::addManualCut(fixture.controller().document(), fixture.placement, 1200);
    fixture.settle();

    GuiFixture::drag(fixture.window().slicer(), fixture.slicerPoint(1200.0), fixture.slicerPoint(1497.0));
    EXPECT_EQ(fixture.project().absolutePulseOf(fixture.project().cutPoint(cut)), 1500);

    GuiFixture::drag(fixture.window().slicer(), fixture.slicerPoint(1500.0), fixture.slicerPoint(1537.0), Qt::AltModifier);
    EXPECT_NEAR(static_cast<double>(fixture.project().absolutePulseOf(fixture.project().cutPoint(cut))), 1537.0, 3.0);
}

TEST(StudioGui, GUI003_DeleteKeyRemovesSelectedCutsButNotTheHead) {
    GuiFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    const auto cut = app::addManualCut(fixture.controller().document(), fixture.placement, 1200);
    const auto head = fixture.project().headCutPointOf(fixture.placement);
    fixture.controller().setSelectedCuts({head, cut});
    fixture.settle();

    QTest::keyClick(fixture.window().slicer(), Qt::Key_Delete);

    EXPECT_EQ(cutPulses(fixture.project(), fixture.placement), std::vector<core::Pulse>{960});
}

TEST(StudioGui, GUI004_DraggingPlacementMovesOrDuplicatesIt) {
    GuiFixture fixture;
    auto* arranger = fixture.window().arranger();
    const QPointF grab = arranger->placementRect(fixture.placement).center();
    const double pixelsPerBar = 960.0 / arranger->viewport().pulsesPerPixel();

    GuiFixture::drag(arranger, grab, grab + QPointF(pixelsPerBar + 2.0, 0.0));
    EXPECT_EQ(fixture.project().placement(fixture.placement).startPulse, 1920);

    const QPointF moved = arranger->placementRect(fixture.placement).center();
    GuiFixture::drag(arranger, moved, moved + QPointF(pixelsPerBar, 0.0), Qt::ControlModifier);
    ASSERT_EQ(fixture.project().placements().size(), 2u);
    EXPECT_EQ(fixture.project().placement(fixture.placement).startPulse, 1920);
    EXPECT_EQ(fixture.project().placements().back().startPulse, 2880);
}

TEST(StudioGui, GUI005_DoubleClickOnEmptyRowPlacesAssetAndUndoRemovesIt) {
    GuiFixture fixture;
    auto* arranger = fixture.window().arranger();
    const double x = ArrangerWidget::kHeaderWidth + arranger->viewport().xOf(5760.0);
    const double y = arranger->rowTop(0) + ArrangerWidget::kRowHeight / 2.0;

    GuiFixture::doubleClick(arranger, QPointF(x, y));

    ASSERT_EQ(fixture.project().placements().size(), 2u);
    EXPECT_EQ(fixture.project().placements().back().startPulse, 5760);
    EXPECT_EQ(fixture.controller().selectedPlacement(), fixture.project().placements().back().id);
    ASSERT_TRUE(fixture.window().undoAction()->isEnabled());
    EXPECT_TRUE(fixture.window().undoAction()->text().contains(QStringLiteral("配置の追加")));
    fixture.window().undoAction()->trigger();
    EXPECT_EQ(fixture.project().placements().size(), 1u);
}

TEST(StudioGui, GUI006_PanelPreviewsAndAppliesDetectedCutsAsOneUndoStep) {
    GuiFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    fixture.settle();
    auto* panel = fixture.window().slicerPanel();
    ASSERT_TRUE(fixture.controller().preview().has_value());
    EXPECT_TRUE(panel->statusLabel()->text().contains(QStringLiteral("立ち上がり 3")));

    panel->sensitivitySlider()->setValue(90);
    EXPECT_DOUBLE_EQ(fixture.controller().sliceSettings().onset.sensitivity, 0.9);
    ASSERT_TRUE(panel->applyButton()->isEnabled());
    panel->applyButton()->click();

    EXPECT_EQ(cutPulses(fixture.project(), fixture.placement), (std::vector<core::Pulse>{960, 1200, 1440, 1680}));
    fixture.window().undoAction()->trigger();
    EXPECT_EQ(cutPulses(fixture.project(), fixture.placement), std::vector<core::Pulse>{960});
}

TEST(StudioGui, GUI007_ClickingASliceAuditionsOnlyThatSlice) {
    GuiFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    app::addManualCut(fixture.controller().document(), fixture.placement, 1200);
    app::addManualCut(fixture.controller().document(), fixture.placement, 1440);
    fixture.settle();

    GuiFixture::send(fixture.window().slicer(), QEvent::MouseButtonPress, fixture.slicerPoint(1300.0), Qt::LeftButton,
                     Qt::LeftButton);

    EXPECT_TRUE(fixture.output().playing);
    EXPECT_EQ(fixture.output().played.frameCount(), 500u);
    ASSERT_EQ(fixture.controller().selectedCuts().size(), 1u);
    EXPECT_EQ(fixture.project().absolutePulseOf(fixture.project().cutPoint(fixture.controller().selectedCuts().front())), 1200);

    QTest::keyClick(fixture.window().slicer(), Qt::Key_Right);
    EXPECT_EQ(fixture.project().absolutePulseOf(fixture.project().cutPoint(fixture.controller().selectedCuts().front())), 1440);
    QTest::keyClick(fixture.window().slicer(), Qt::Key_Space);
    EXPECT_FALSE(fixture.output().playing);
}

}  // namespace
}  // namespace ibs::gui::testing
