#include <QAbstractButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

#include "BgaPanel.hpp"
#include "ChartEditorWidget.hpp"
#include "GuiFixture.hpp"
#include "NullTestDialog.hpp"
#include "SongSettingsDialog.hpp"
#include "StatisticsDialog.hpp"
#include "TimelineScrollBar.hpp"
#include "WaveformSlicerWidget.hpp"
#include "ibs/core/timing/TimingEdits.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::gui::testing {
namespace {

class ChartFixture : public GuiFixture {
public:
    ChartFixture() {
        for (const core::Pulse pulse : {1200, 1440, 1680}) {
            cuts.push_back(app::addManualCut(controller().document(), placement, pulse));
        }
        window().tabs()->setCurrentIndex(1);
        editor()->viewport().setPulsesPerPixel(2.0);
        editor()->viewport().setOrigin(1000.0);
        settle();
        EXPECT_GT(editor()->yOf(1700.0), ChartEditorWidget::kHeaderHeight) << "the cut points must be visible";
    }

    ChartEditorWidget* editor() { return window().chartEditor(); }

    QPointF cutMark(core::CutPointId cut) {
        const auto column = editor()->assetColumn(asset);
        const auto pulse = project().absolutePulseOf(project().cutPoint(cut));
        return {column->left + column->width / 2.0, editor()->yOf(static_cast<double>(pulse))};
    }

    QPointF noteHead(core::NoteId note) {
        const auto column = editor()->laneColumn(project().note(note).lane);
        const auto pulse = project().absolutePulseOf(project().cutPoint(project().note(note).cutPointId));
        return {column->left + column->width / 2.0, editor()->yOf(static_cast<double>(pulse)) - 3.0};
    }

    std::vector<core::CutPointId> cuts;
};

TEST(ChartEditorGui, GUI008_NumberKeyAssignsTheSelectedSliceAndAdvances) {
    ChartFixture fixture;
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonPress, fixture.cutMark(fixture.cuts[0]), Qt::LeftButton, Qt::LeftButton);
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonRelease, fixture.cutMark(fixture.cuts[0]), Qt::LeftButton, Qt::NoButton);
    EXPECT_TRUE(fixture.output().playing);

    QTest::keyClick(fixture.editor(), Qt::Key_3);

    ASSERT_EQ(fixture.project().notesOf(fixture.cuts[0]).size(), 1u);
    EXPECT_EQ(fixture.project().note(fixture.project().notesOf(fixture.cuts[0]).front()).lane, 3);
    EXPECT_EQ(fixture.controller().selectedCuts(), std::vector<core::CutPointId>{fixture.cuts[1]});
    QTest::keyClick(fixture.editor(), Qt::Key_S);
    EXPECT_EQ(fixture.project().note(fixture.project().notesOf(fixture.cuts[1]).front()).lane, 8);
    QTest::keyClick(fixture.editor(), Qt::Key_Down);
    QTest::keyClick(fixture.editor(), Qt::Key_0);
    EXPECT_TRUE(fixture.project().notesOf(fixture.cuts[1]).empty());
}

TEST(ChartEditorGui, GUI009_DraggingANoteChangesItsLaneOrLength) {
    ChartFixture fixture;
    const auto note = app::assignLanes(fixture.controller().document(), {fixture.cuts[0]}, 2).front();
    fixture.settle();
    const auto target = fixture.editor()->laneColumn(6);

    const QPointF head = fixture.noteHead(note);
    GuiFixture::drag(fixture.editor(), head, QPointF(target->left + target->width / 2.0, head.y()));
    EXPECT_EQ(fixture.project().note(note).lane, 6);

    const QPointF moved = fixture.noteHead(note);
    GuiFixture::drag(fixture.editor(), moved, QPointF(moved.x(), fixture.editor()->yOf(1438.0)), Qt::ShiftModifier);
    EXPECT_EQ(fixture.project().note(note).lengthPulses, 240);
}

TEST(ChartEditorGui, GUI010_CopiedPatternIsPastedAtTheCursor) {
    ChartFixture fixture;
    const auto note = app::assignLanes(fixture.controller().document(), {fixture.cuts[0]}, 4).front();
    fixture.controller().setSelectedNotes({note});
    QTest::keyClick(fixture.editor(), Qt::Key_C, Qt::ControlModifier);

    const QPointF empty(fixture.editor()->laneColumn(1)->left + 5.0, fixture.editor()->yOf(1441.0));
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonPress, empty, Qt::LeftButton, Qt::LeftButton);
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonRelease, empty, Qt::LeftButton, Qt::NoButton);
    EXPECT_EQ(fixture.controller().cursorPulse(), 1440);
    QTest::keyClick(fixture.editor(), Qt::Key_V, Qt::ControlModifier);

    ASSERT_EQ(fixture.project().notesOf(fixture.cuts[1]).size(), 1u);
    EXPECT_EQ(fixture.project().note(fixture.project().notesOf(fixture.cuts[1]).front()).lane, 4);
}

TEST(ChartEditorGui, GUI011_RubberBandSelectsNotesAndCuts) {
    ChartFixture fixture;
    const auto notes = app::assignLanes(fixture.controller().document(), {fixture.cuts[0], fixture.cuts[1]}, 1);
    fixture.settle();

    const QPointF from(fixture.editor()->laneColumn(1)->left - 2.0, fixture.editor()->yOf(1460.0));
    const QPointF to(fixture.editor()->assetColumn(fixture.asset)->left + 30.0, fixture.editor()->yOf(1100.0));
    GuiFixture::drag(fixture.editor(), from, to);

    EXPECT_EQ(fixture.controller().selectedNotes(), notes);
    EXPECT_EQ(fixture.controller().selectedCuts(), (std::vector<core::CutPointId>{fixture.cuts[0], fixture.cuts[1]}));
}

TEST(ChartEditorGui, GUI012_TempoChangeAsksAndKeepsCutTimes) {
    ChartFixture fixture;
    QTimer::singleShot(0, [] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        ASSERT_NE(box, nullptr);
        for (auto* button : box->buttons()) {
            if (button->text() == QStringLiteral("時間を保つ")) {
                button->click();
            }
        }
    });

    fixture.controller().editTiming(QStringLiteral("BPM の変更"),
                                    [](core::TimingData& timing) { core::setBpmChange(timing, 1320, 60.0); });

    EXPECT_EQ(fixture.project().absolutePulseOf(fixture.project().cutPoint(fixture.cuts[1])), 1380);
    EXPECT_TRUE(fixture.project().tempoAffectedPlacements().empty());
    EXPECT_EQ(fixture.controller().document().undoLabel(), "切断点の時間を保つ");
}

TEST(ChartEditorGui, GUI013_CutModeAddsAndMovesCutPoints) {
    ChartFixture fixture;
    QTest::keyClick(fixture.editor(), Qt::Key_Tab);
    ASSERT_EQ(fixture.editor()->mode(), ChartEditMode::CutPoints);
    const auto column = fixture.editor()->assetColumn(fixture.asset);

    GuiFixture::doubleClick(fixture.editor(), QPointF(column->left + 10.0, fixture.editor()->yOf(1322.0)));
    ASSERT_EQ(fixture.project().cutPointsOf(fixture.placement).size(), 5u);

    GuiFixture::drag(fixture.editor(), fixture.cutMark(fixture.cuts[2]), QPointF(column->left + 10.0, fixture.editor()->yOf(1562.0)));
    EXPECT_EQ(fixture.project().absolutePulseOf(fixture.project().cutPoint(fixture.cuts[2])), 1560);
}

TEST(ChartEditorGui, GUI014_BgaImagesArePlacedMovedAndRemovedInTheirLanes) {
    ChartFixture fixture;
    ASSERT_TRUE(fixture.window().bgaPanel()->addImage(kChartDirectory / "movie" / "bg.png"));
    EXPECT_FALSE(fixture.window().bgaPanel()->addImage(std::filesystem::path("elsewhere") / "bg.png"));
    ASSERT_EQ(fixture.window().bgaPanel()->list()->count(), 1);
    ASSERT_EQ(fixture.controller().selectedBgaImage(), std::optional<std::uint32_t>(1));
    const auto column = fixture.editor()->bgaColumn(core::BgaTrack::Layer);

    GuiFixture::doubleClick(fixture.editor(), QPointF(column->left + 10.0, fixture.editor()->yOf(1205.0)));
    ASSERT_EQ(fixture.project().bga.layerEvents, (std::vector<core::BgaEvent>{{1200, 1}}));
    EXPECT_EQ(fixture.project().bga.headers.front().name, "movie/bg.png");

    GuiFixture::drag(fixture.editor(), QPointF(column->left + 10.0, fixture.editor()->yOf(1200.0) - 5.0),
                     QPointF(column->left + 10.0, fixture.editor()->yOf(1440.0) - 5.0));
    ASSERT_EQ(fixture.project().bga.layerEvents.front().y, 1440);
    QTest::keyClick(fixture.editor(), Qt::Key_Delete);
    EXPECT_TRUE(fixture.project().bga.layerEvents.empty());
}

TEST(ChartEditorGui, GUI015_ExternalPreviewStartsFromTheCursorMeasure) {
    ChartFixture fixture;
    fixture.controller().setPlayerSettings(app::PlayerSettings{"Pulsus.exe"});
    fixture.controller().setCursorPulse(1000);

    const auto outcome = fixture.controller().previewInPlayer();

    ASSERT_TRUE(outcome.launched) << outcome.error;
    ASSERT_EQ(fixture.launcher().requests.size(), 1u);
    const auto& request = fixture.launcher().requests.front();
    EXPECT_EQ(request.program, "Pulsus.exe");
    EXPECT_EQ(request.arguments, (std::vector<std::string>{infra::pathToUtf8(kChartDirectory / ".~preview_song.bmson"), "-p", "-m", "1"}));
}

TEST(ChartEditorGui, GUI016_SongInfoImagesMustBeInsideTheChartFolder) {
    ChartFixture fixture;
    SongSettingsDialog dialog(SongSettingsDialog::Purpose::EditSong, fixture.project(),
                              SongSettingsContext{kChartDirectory, {"beat-7k"}, 460.9});

    dialog.acceptImage(QStringLiteral("banner_image"), kChartDirectory / "img" / "banner.png");
    dialog.acceptImage(QStringLiteral("back_image"), std::filesystem::path("elsewhere") / "back.png");
    dialog.titleField()->setText(QStringLiteral("いぐさの歌"));
    const auto result = dialog.result();
    fixture.controller().updateSongInfo(result.info, result.initialBpm);

    EXPECT_EQ(fixture.project().info.bannerImage, "img/banner.png");
    EXPECT_TRUE(fixture.project().info.backImage.empty());
    EXPECT_EQ(fixture.project().info.title, "いぐさの歌");
    EXPECT_EQ(fixture.controller().document().undoLabel(), "曲の情報");
}

TEST(ChartEditorGui, GUI017_ScrollBarsMoveTheArrangerAndWaveform) {
    ChartFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    fixture.settle();
    auto* slicer = fixture.window().slicer();
    slicer->viewport().setPulsesPerPixel(0.5);
    slicer->repaint();

    TimelineScrollBar* slicerBar = nullptr;
    for (auto* bar : fixture.window().findChildren<TimelineScrollBar*>()) {
        if (bar->parentWidget() == slicer->parentWidget()) {
            slicerBar = bar;
        }
    }
    ASSERT_NE(slicerBar, nullptr);
    EXPECT_GT(slicerBar->maximum(), slicerBar->minimum());
    const int target = (slicerBar->minimum() + slicerBar->maximum()) / 2;
    slicerBar->setValue(target);
    EXPECT_DOUBLE_EQ(slicer->viewport().originPulse(), static_cast<double>(target));
    EXPECT_EQ(fixture.window().findChildren<TimelineScrollBar*>().size(), 2u);
}

TEST(ChartEditorGui, GUI018_ArrowKeysScrollTheNextSliceIntoView) {
    ChartFixture fixture;
    fixture.controller().selectPlacement(fixture.placement);
    fixture.settle();
    auto* slicer = fixture.window().slicer();
    slicer->viewport().setPulsesPerPixel(0.5);
    slicer->viewport().setOrigin(960.0);
    fixture.controller().setSelectedCuts({fixture.cuts[1]});

    QTest::keyClick(slicer, Qt::Key_Right);

    const double origin = slicer->viewport().originPulse();
    const double visibleEnd = origin + slicer->width() * slicer->viewport().pulsesPerPixel();
    EXPECT_EQ(fixture.controller().selectedCuts(), std::vector<core::CutPointId>{fixture.cuts[2]});
    EXPECT_LE(origin, 1680.0);
    EXPECT_GE(visibleEnd, 1680.0 + 240.0);
    EXPECT_DOUBLE_EQ(slicer->viewport().pulsesPerPixel(), 0.5);
}

TEST(ChartEditorGui, GUI019_ClickingANoteShowsItsSliceUnlessTurnedOff) {
    ChartFixture fixture;
    const auto note = app::assignLanes(fixture.controller().document(), {fixture.cuts[1]}, 2).front();
    const auto other = app::placeAsset(fixture.controller().document(), fixture.asset, 9600);
    fixture.controller().selectPlacement(other);
    fixture.settle();

    fixture.controller().setFollowsChartSelection(false);
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonPress, fixture.noteHead(note), Qt::LeftButton, Qt::LeftButton);
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonRelease, fixture.noteHead(note), Qt::LeftButton, Qt::NoButton);
    EXPECT_EQ(fixture.controller().selectedPlacement(), other);

    fixture.controller().setFollowsChartSelection(true);
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonPress, fixture.noteHead(note), Qt::LeftButton, Qt::LeftButton);
    GuiFixture::send(fixture.editor(), QEvent::MouseButtonRelease, fixture.noteHead(note), Qt::LeftButton, Qt::NoButton);
    fixture.settle();

    auto* slicer = fixture.window().slicer();
    const double origin = slicer->viewport().originPulse();
    const double visibleEnd = origin + slicer->width() * slicer->viewport().pulsesPerPixel();
    EXPECT_EQ(fixture.controller().selectedPlacement(), fixture.placement);
    EXPECT_LE(origin, 1440.0);
    EXPECT_GE(visibleEnd, 1680.0);
    EXPECT_LT(visibleEnd - origin, 2000.0);
}

TEST(ChartEditorGui, GUI020_StatisticsDialogShowsChartBgmAndTotal) {
    ChartFixture fixture;
    app::assignLanes(fixture.controller().document(), {fixture.cuts[0]}, 8);
    StatisticsDialog dialog(fixture.controller());
    fixture.settle();
    auto* table = dialog.table();

    ASSERT_GT(table->rowCount(), 10);
    EXPECT_EQ(table->item(0, 1)->text(), QStringLiteral("1"));
    EXPECT_EQ(table->item(0, 2)->text(), QStringLiteral("3"));
    EXPECT_EQ(table->item(0, 3)->text(), QStringLiteral("4"));
    EXPECT_EQ(table->item(3, 1)->text(), QStringLiteral("1"));
    int peakRow = -1;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)->text().startsWith(QStringLiteral("最大音量"))) {
            peakRow = row;
        }
    }
    ASSERT_GE(peakRow, 0);
    EXPECT_TRUE(table->item(peakRow, 3)->text().contains(QStringLiteral("小節")));
    emit table->cellDoubleClicked(peakRow, 3);
    EXPECT_GE(fixture.controller().cursorPulse(), 960);
}

TEST(ChartEditorGui, GUI021_NullTestDialogListsTheDoubledSectionAndJumpsToIt) {
    ChartFixture fixture;
    app::assignLanes(fixture.controller().document(), {fixture.cuts[0]}, 1);
    fixture.controller().document().apply("二重", [&](core::Project& project) {
        return project.addNote(fixture.cuts[0], 2, 0);
    });
    fixture.controller().setCursorPulse(0);

    NullTestDialog dialog(fixture.controller(), NullTestSource::InternalPlayer);
    fixture.settle();

    EXPECT_TRUE(dialog.summary()->text().startsWith(QStringLiteral("不一致"))) << dialog.summary()->text().toStdString();
    ASSERT_GT(dialog.table()->rowCount(), 0);
    emit dialog.table()->cellDoubleClicked(0, 0);
    EXPECT_GE(fixture.controller().cursorPulse(), 960);
}

TEST(ChartEditorGui, GUI022_NullTestWithPlayerReportsRenderFailures) {
    ChartFixture fixture;
    {
        NullTestDialog dialog(fixture.controller(), NullTestSource::ExternalPlayer);
        fixture.settle();
        EXPECT_TRUE(dialog.summary()->text().contains(QStringLiteral("プレーヤー")));
        EXPECT_TRUE(fixture.runner().requests.empty());
    }
    auto player = fixture.controller().playerSettings();
    player.executable = "Pulsus.exe";
    fixture.controller().setPlayerSettings(player);
    fixture.runner().exitCode = 5;

    NullTestDialog dialog(fixture.controller(), NullTestSource::ExternalPlayer);
    fixture.settle();

    ASSERT_EQ(fixture.runner().requests.size(), 1u);
    EXPECT_EQ(fixture.runner().requests.front().arguments.at(1), "--render");
    EXPECT_TRUE(dialog.summary()->text().contains(QStringLiteral("5")));
}

}  // namespace
}  // namespace ibs::gui::testing
