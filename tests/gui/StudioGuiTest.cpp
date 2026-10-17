#include <functional>

#include <QAbstractButton>
#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>

#include <gtest/gtest.h>

#include "AboutDialog.hpp"
#include "AppIcon.hpp"
#include "ArrangerWidget.hpp"
#include "CutLayoutCopyDialog.hpp"
#include "GuiFixture.hpp"
#include "SlicerPanel.hpp"
#include "StudioSettings.hpp"
#include "StudioSettingsDialog.hpp"
#include "ibs/core/BuildInfo.hpp"

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

TEST(StudioGui, GUI023_ArrangerTurnsThePageWhilePlayingUnlessTurnedOff) {
    GuiFixture fixture;
    auto& viewport = fixture.window().arranger()->viewport();
    viewport.setPulsesPerPixel(1.0);
    viewport.setOrigin(0.0);
    fixture.controller().setCursorPulse(0);
    fixture.controller().setFollowsPlayback(false);
    fixture.controller().playSongFromCursor();
    fixture.output().position = 3000;

    emit fixture.controller().playheadChanged();
    EXPECT_DOUBLE_EQ(viewport.originPulse(), 0.0);

    fixture.controller().setFollowsPlayback(true);
    emit fixture.controller().playheadChanged();
    EXPECT_GT(viewport.originPulse(), 1000.0);
    EXPECT_LT(viewport.originPulse(), 1440.0);
}

TEST(StudioGui, GUI024_RecentFilesMenuListsTheNewestFirst) {
    GuiFixture fixture;
    openStudioSettings()->setValue(QStringLiteral("file/recentFiles"), QStringList{QStringLiteral("b.bmson"), QStringLiteral("a.bmson")});

    emit fixture.window().recentMenu()->aboutToShow();

    const auto actions = fixture.window().recentMenu()->actions();
    ASSERT_EQ(actions.size(), 2);
    EXPECT_EQ(actions.front()->text(), QStringLiteral("b.bmson"));
    EXPECT_TRUE(fixture.window().recentMenu()->isEnabled());
    openStudioSettings()->remove(QStringLiteral("file/recentFiles"));
}

TEST(StudioGui, GUI025_AuditioningASliceMovesOnlyTheWaveformPlayhead) {
    GuiFixture fixture;
    auto& viewport = fixture.window().arranger()->viewport();
    viewport.setPulsesPerPixel(1.0);
    viewport.setOrigin(0.0);
    fixture.controller().setFollowsPlayback(true);

    fixture.controller().playSlice(fixture.project().headCutPointOf(fixture.placement));
    fixture.output().position = 3000;
    emit fixture.controller().playheadChanged();

    EXPECT_TRUE(fixture.controller().playheadPulse().has_value());
    EXPECT_FALSE(fixture.controller().songPlayheadPulse().has_value());
    EXPECT_DOUBLE_EQ(viewport.originPulse(), 0.0);

    fixture.controller().playSongFromCursor();
    EXPECT_TRUE(fixture.controller().songPlayheadPulse().has_value());
}

TEST(StudioGui, GUI026_AboutDialogShowsIconVersionAndLicenses) {
    AboutDialog dialog;

    const QPixmap icon = dialog.iconLabel()->pixmap();
    EXPECT_EQ(icon.width(), 128);
    EXPECT_EQ(icon.height(), 128);
    EXPECT_TRUE(dialog.versionLabel()->text().contains(QString::fromLatin1(core::kVersion.data(), static_cast<int>(core::kVersion.size()))));
    const QString notices = dialog.notices()->toPlainText();
    EXPECT_TRUE(notices.contains(QStringLiteral("LGPLv3")));
    EXPECT_TRUE(notices.contains(QStringLiteral("miniaudio")));
    EXPECT_FALSE(appIcon().availableSizes().isEmpty());
}

TEST(StudioGui, GUI027_SettingsAreKeptInAnIniFileThatCanBeEditedByHand) {
    app::PlayerSettings player;
    player.executable = QDir::toNativeSeparators(QStringLiteral("C:/Games/Pulsus/Pulsus.exe")).toStdString();
    StudioSettingsDialog::save(player);

    QFile file(studioSettingsFile());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(file.readAll());
    EXPECT_TRUE(text.contains(QStringLiteral("[preview]")));
    EXPECT_TRUE(text.contains(QStringLiteral("executable=C:/Games/Pulsus/Pulsus.exe")));
    EXPECT_EQ(StudioSettingsDialog::load().executable, player.executable);
    EXPECT_TRUE(studioSettingsWritable());
    file.close();
    QFile::remove(studioSettingsFile());
}

}  // namespace

namespace {

void answerNextMessageBox(QMessageBox::StandardButton button, QString* text = nullptr) {
    QTimer::singleShot(0, [button, text] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        ASSERT_NE(box, nullptr);
        if (text != nullptr) {
            *text = box->text();
        }
        box->button(button)->click();
    });
}

void chooseFromContextMenu(QWidget* widget, QPoint at, const QString& actionText, const std::function<void()>& before = {}) {
    QTimer::singleShot(0, [actionText, before] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        ASSERT_NE(menu, nullptr);
        QAction* chosen = nullptr;
        for (auto* action : menu->actions()) {
            if (action->text() == actionText) {
                chosen = action;
            }
        }
        ASSERT_NE(chosen, nullptr) << actionText.toStdString();
        if (before) {
            before();
        }
        menu->hide();
        chosen->trigger();
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, at, widget->mapToGlobal(at));
    QApplication::sendEvent(widget, &event);
}

QPoint assetHeader(GuiFixture& fixture, std::size_t row) {
    return QPoint(30, fixture.window().arranger()->rowTop(row) + ArrangerWidget::kRowHeight / 2);
}

}  // namespace

TEST(StudioGui, GUI028_RemovingAnAssetFromItsHeaderAsksFirst) {
    GuiFixture fixture;
    QString question;
    chooseFromContextMenu(fixture.window().arranger(), assetHeader(fixture, 0), QStringLiteral("音源定義を削除する"),
                          [&question] { answerNextMessageBox(QMessageBox::No, &question); });
    EXPECT_EQ(fixture.project().assets().size(), 1u);
    EXPECT_TRUE(question.contains(QStringLiteral("音源参照をbmsonファイルから削除しますか？ノーツの設定は無くなります(音源は消えません)")));

    chooseFromContextMenu(fixture.window().arranger(), assetHeader(fixture, 0), QStringLiteral("音源定義を削除する"),
                          [] { answerNextMessageBox(QMessageBox::Yes); });

    EXPECT_TRUE(fixture.project().assets().empty());
    EXPECT_TRUE(fixture.project().placements().empty());
    EXPECT_EQ(fixture.controller().document().undoLabel(), "素材の削除");
}

TEST(StudioGui, GUI029_CopiesCutPositionsToAnAssetPlacedAtTheSameSpots) {
    GuiFixture fixture;
    const auto [hats, other] = fixture.controller().document().apply("素材", [](core::Project& project) {
        const auto first = project.addAsset("hats.wav");
        const auto second = project.addAsset("other.wav");
        project.addPlacement(first, 960, 1);
        project.addPlacement(second, 1920, 2);
        return std::pair{first, second};
    });
    app::addManualCut(fixture.controller().document(), fixture.placement, 1200);
    app::addManualCut(fixture.controller().document(), fixture.placement, 1440);
    QString confirmation;
    int listed = -1;

    chooseFromContextMenu(fixture.window().arranger(), assetHeader(fixture, 0), QStringLiteral("音源配置をコピーする…"), [&] {
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<CutLayoutCopyDialog*>(QApplication::activeModalWidget());
            ASSERT_NE(dialog, nullptr);
            listed = dialog->list()->count();
            EXPECT_FALSE(dialog->okButton()->isEnabled());
            dialog->list()->item(0)->setCheckState(Qt::Checked);
            ASSERT_TRUE(dialog->okButton()->isEnabled());
            answerNextMessageBox(QMessageBox::Yes, &confirmation);
            dialog->okButton()->click();
        });
    });

    EXPECT_EQ(listed, 1);
    EXPECT_EQ(confirmation, QStringLiteral("既存のノーツの設定は上書きされますがよろしいですか？"));
    const auto& project = fixture.project();
    for (const auto& placement : project.placements()) {
        if (placement.assetId == hats) {
            EXPECT_EQ(project.cutPointsOf(placement.id).size(), 3u);
        }
        if (placement.assetId == other) {
            EXPECT_EQ(project.cutPointsOf(placement.id).size(), 1u);
        }
    }
    EXPECT_EQ(fixture.controller().document().undoLabel(), "切断点のコピー");
}

}  // namespace ibs::gui::testing
