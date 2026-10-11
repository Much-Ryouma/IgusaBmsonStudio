#include <QAbstractButton>
#include <QMessageBox>
#include <QTabWidget>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

#include "BgaPanel.hpp"
#include "ChartEditorWidget.hpp"
#include "SongSettingsDialog.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"
#include <QLineEdit>
#include <QListWidget>
#include "GuiFixture.hpp"
#include "ibs/core/timing/TimingEdits.hpp"

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

}  // namespace
}  // namespace ibs::gui::testing
