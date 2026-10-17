#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollBar>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QWidget>

#include <nlohmann/json.hpp>

#include "AboutDialog.hpp"
#include "ArrangerWidget.hpp"
#include "BgaPanel.hpp"
#include "ChartEditorWidget.hpp"
#include "ChartMinimap.hpp"
#include "MainWindow.hpp"
#include "NullTestDialog.hpp"
#include "QtProcessLauncher.hpp"
#include "QtTranslations.hpp"
#include "SlicerPanel.hpp"
#include "SongSettingsDialog.hpp"
#include "StudioSettings.hpp"
#include "StatisticsDialog.hpp"
#include "StudioController.hpp"
#include "StudioSettingsDialog.hpp"
#include "WaveformSlicerWidget.hpp"
#include "ibs/app/ArrangementCommands.hpp"
#include "ibs/app/AssetImportService.hpp"
#include "ibs/app/BgaCommands.hpp"
#include "ibs/app/ChartCommands.hpp"
#include "ibs/app/ChartStorage.hpp"
#include "ibs/app/SlicingService.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/mode/KeyModeJsonLoader.hpp"

namespace {

using namespace ibs;
using Json = nlohmann::ordered_json;

class SilentOutput final : public core::AudioOutputPort {
public:
    int sampleRate() const override { return 44100; }
    bool start(core::AudioBuffer) override { return true; }
    void stop() override {}
    bool isPlaying() const override { return false; }
    core::SampleIndex playedFrames() const override { return 0; }
};

class NoProcess final : public app::ProcessRunner {
public:
    int run(const app::LaunchRequest&) override { return 1; }
};

struct Shot {
    std::filesystem::path directory;
    Json regions = Json::object();

    void save(QWidget* widget, const std::string& name) {
        widget->grab().save(QString::fromStdString(infra::pathToUtf8(directory / (name + ".png"))));
    }

    void region(const std::string& image, const std::string& id, QWidget* root, QWidget* part) {
        const QPoint origin = part->mapTo(root, QPoint(0, 0));
        add(image, id, QRect(origin, part->size()));
    }

    void add(const std::string& image, const std::string& id, QRect rect) {
        regions[image][id] = Json::array({rect.x(), rect.y(), rect.width(), rect.height()});
    }

    void write() {
        std::ofstream(directory / "regions.json") << regions.dump(2) << "\n";
    }
};

void settle(gui::StudioController& controller) {
    for (int round = 0; round < 400; ++round) {
        controller.waitForBackgroundWork();
        QApplication::processEvents();
        if (!controller.isAnalyzing()) {
            break;
        }
    }
    QApplication::processEvents();
}

std::vector<core::CutPointId> cutsOf(const core::Project& project, core::PlacementId placement) {
    return project.cutPointsOf(placement);
}

QRect columnRect(const gui::ChartEditorWidget::Column& column, QWidget* editor, QWidget* root) {
    const QPoint origin = editor->mapTo(root, QPoint(0, 0));
    return QRect(origin.x() + static_cast<int>(column.left), origin.y(), static_cast<int>(column.width), editor->height());
}

}  // namespace

int main(int argc, char** argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication application(argc, argv);
    if (argc < 4) {
        std::cerr << "usage: ibs_wiki_screenshots <demo folder> <modes folder> <output folder>\n";
        return 2;
    }
    QFont font(QStringLiteral("Noto Sans CJK JP"));
    font.setPointSize(9);
    QApplication::setFont(font);
    gui::installQtJapaneseTranslation(application);

    const auto demo = infra::pathFromUtf8(argv[1]);
    const auto modes = infra::loadKeyModeCatalog(infra::pathFromUtf8(argv[2]));
    Shot shot{infra::pathFromUtf8(argv[3])};
    std::filesystem::create_directories(shot.directory);
    gui::setStudioSettingsFile(QString::fromStdString(infra::pathToUtf8(shot.directory / "screenshots.ini")));

    app::LocalChartStorage storage;
    app::LocalAudioSource audioSource;
    SilentOutput output;
    app::LocalAssetFileStore assetFiles;
    gui::QtProcessLauncher launcher;
    NoProcess runner;
    gui::StudioController controller(gui::StudioPorts{storage, audioSource, output, assetFiles, modes, launcher, runner});
    gui::MainWindow window(controller);
    window.resize(1400, 900);

    gui::NewProjectSettings settings;
    settings.chartPath = demo / "demo.bmson";
    settings.info.title = "Demo Song";
    settings.info.artist = "IgusaBmsonStudio";
    settings.info.genre = "Demo";
    settings.info.chartName = "HYPER";
    settings.info.level = 7;
    settings.initialBpm = 150.0;
    controller.newProject(settings);

    std::vector<core::AssetId> assets;
    for (const char* name : {"drums.wav", "bass.wav", "lead.wav"}) {
        const auto imported = controller.importAudio(demo / name, app::OutsideFilePolicy::Reject);
        if (!imported.assetId) {
            std::cerr << "import failed: " << name << " " << imported.error << "\n";
            return 1;
        }
        assets.push_back(*imported.assetId);
    }
    const int resolution = controller.project().timing.resolution;
    const core::Pulse bar = resolution * 4;
    const auto drums = app::placeAsset(controller.document(), assets[0], 0);
    const auto bass = app::placeAsset(controller.document(), assets[1], 0);
    const auto lead = app::placeAsset(controller.document(), assets[2], 2 * bar);
    window.show();
    settle(controller);
    window.arranger()->zoomToFit();

    for (const auto placement : {drums, lead}) {
        controller.selectPlacement(placement);
        controller.requestPreview();
        settle(controller);
        static_cast<void>(controller.applyPreview());
        settle(controller);
    }

    controller.selectPlacement(bass);
    controller.requestPreview();
    settle(controller);
    window.slicer()->setFocus();
    QApplication::processEvents();
    shot.save(&window, "slicer_preview_full");
    {
        QWidget* bottom = window.slicer()->parentWidget()->parentWidget();
        shot.save(bottom, "slicer_preview");
        shot.region("slicer_preview", "waveform", bottom, window.slicer());
        shot.region("slicer_preview", "panel", bottom, window.slicerPanel());
        auto* form = window.slicerPanel()->findChild<QFormLayout*>();
        if (form != nullptr) {
            for (int row = 0; row < form->rowCount(); ++row) {
                auto* label = form->itemAt(row, QFormLayout::LabelRole);
                auto* field = form->itemAt(row, QFormLayout::FieldRole);
                if (label != nullptr && field != nullptr && label->widget() != nullptr) {
                    const QString text = qobject_cast<QLabel*>(label->widget())->text();
                    QRect rect = label->geometry().united(field->geometry());
                    rect.translate(window.slicerPanel()->mapTo(bottom, QPoint(0, 0)));
                    shot.add("slicer_preview", "param:" + text.toStdString(), rect);
                }
            }
        }
        for (auto* button : window.slicerPanel()->findChildren<QPushButton*>()) {
            shot.region("slicer_preview", "button:" + button->text().toStdString(), bottom, button);
        }
        shot.region("slicer_preview", "status", bottom, window.slicerPanel()->statusLabel());
    }
    static_cast<void>(controller.applyPreview());
    settle(controller);

    const auto& project = controller.project();
    std::vector<core::CutPointId> kicks;
    std::vector<core::CutPointId> snares;
    std::vector<core::CutPointId> hats;
    for (const auto cut : cutsOf(project, drums)) {
        const auto pulse = project.absolutePulseOf(project.cutPoint(cut));
        if (pulse % (2 * resolution) == 0) {
            kicks.push_back(cut);
        } else if (pulse % resolution == 0) {
            snares.push_back(cut);
        } else {
            hats.push_back(cut);
        }
    }
    app::assignLanes(controller.document(), kicks, 1);
    app::assignLanes(controller.document(), snares, 3);
    app::assignLanes(controller.document(), hats, 7);
    const auto leadCuts = cutsOf(controller.project(), lead);
    for (std::size_t index = 0; index < leadCuts.size(); ++index) {
        app::assignLanes(controller.document(), {leadCuts[index]}, index % 2 == 0 ? 2 : 4);
    }
    const auto bassCuts = cutsOf(controller.project(), bass);
    for (std::size_t index = 0; index < bassCuts.size(); index += 8) {
        app::assignLanes(controller.document(), {bassCuts[index]}, 8);
    }
    const auto image = app::addBgaImage(controller.document(), "img/bg.png");
    app::placeBgaEvent(controller.document(), core::BgaTrack::Bga, 0, image);
    auto info = controller.project().info;
    info.bannerImage = "img/banner.png";
    app::updateSongInfo(controller.document(), info);
    settle(controller);

    controller.selectPlacement(drums);
    {
        const auto manual = app::addManualCut(controller.document(), drums, bar + resolution / 4 * 3);
        static_cast<void>(manual);
        const auto drumCuts = cutsOf(controller.project(), drums);
        controller.setSelectedCuts({drumCuts[10]});
        settle(controller);
        window.slicer()->viewport().setPulsesPerPixel(static_cast<double>(2 * bar) / window.slicer()->width());
        window.slicer()->viewport().setOrigin(static_cast<double>(bar));
        window.slicer()->repaint();
        QWidget* bottom = window.slicer()->parentWidget()->parentWidget();
        shot.save(bottom, "slicer_cuts");
    }
    controller.setCursorPulse(bar);
    window.tabs()->setCurrentIndex(0);
    window.arranger()->zoomToFit();
    settle(controller);
    shot.save(&window, "main_arranger");
    shot.region("main_arranger", "menubar", &window, window.menuBar());
    for (auto* toolbar : window.findChildren<QToolBar*>()) {
        shot.region("main_arranger", "toolbar", &window, toolbar);
    }
    shot.region("main_arranger", "tabs", &window, window.tabs()->tabBar());
    shot.region("main_arranger", "arranger", &window, window.arranger());
    shot.add("main_arranger", "arranger_header",
             QRect(window.arranger()->mapTo(&window, QPoint(0, 0)), QSize(gui::ArrangerWidget::kHeaderWidth, window.arranger()->height())));
    {
        const QRectF block = window.arranger()->placementRect(drums);
        shot.add("main_arranger", "placement",
                 block.toRect().translated(window.arranger()->mapTo(&window, QPoint(0, 0))));
    }
    shot.add("main_arranger", "ruler",
             QRect(window.arranger()->mapTo(&window, QPoint(gui::ArrangerWidget::kHeaderWidth, 0)),
                   QSize(window.arranger()->width() - gui::ArrangerWidget::kHeaderWidth, gui::ArrangerWidget::kRulerHeight)));
    for (auto* scroll : window.findChildren<QScrollBar*>()) {
        if (scroll->parentWidget() == window.arranger()->parentWidget() && scroll->orientation() == Qt::Horizontal) {
            shot.region("main_arranger", "arranger_scroll", &window, scroll);
        }
        if (scroll->parentWidget() == window.slicer()->parentWidget() && scroll->orientation() == Qt::Horizontal) {
            shot.region("main_arranger", "waveform_scroll", &window, scroll);
        }
    }
    shot.region("main_arranger", "waveform", &window, window.slicer());
    shot.region("main_arranger", "slicer_panel", &window, window.slicerPanel());
    shot.region("main_arranger", "statusbar", &window, window.statusBar());

    window.tabs()->setCurrentIndex(1);
    QApplication::processEvents();
    auto* editor = window.chartEditor();
    editor->viewport().setPulsesPerPixel(3.0);
    editor->viewport().setOrigin(static_cast<double>(bar * 2 - resolution));
    controller.setCursorPulse(bar * 3);
    controller.selectPlacement(lead);
    controller.setSelectedCuts({leadCuts[4]});
    controller.focusCuts({leadCuts[4]});
    settle(controller);
    shot.save(&window, "main_chart");
    const auto columns = editor->columns();
    QRect lanes;
    QRect bgm;
    QRect bga;
    QRect assetColumns;
    for (const auto& column : columns) {
        const QRect rect = columnRect(column, editor, &window);
        using Kind = gui::ChartEditorWidget::Column::Kind;
        QRect* target = column.kind == Kind::Lane ? &lanes : column.kind == Kind::BgmColumn ? &bgm : column.kind == Kind::Bga ? &bga : &assetColumns;
        *target = target->isNull() ? rect : target->united(rect);
    }
    const QPoint editorOrigin = editor->mapTo(&window, QPoint(0, 0));
    using Editor = gui::ChartEditorWidget;
    shot.add("main_chart", "measures", QRect(editorOrigin, QSize(Editor::kRulerWidth, editor->height())));
    shot.add("main_chart", "timing", QRect(editorOrigin + QPoint(Editor::kRulerWidth, 0), QSize(Editor::kTimingWidth, editor->height())));
    shot.add("main_chart", "lanes", lanes);
    shot.add("main_chart", "bgm_columns", bgm);
    shot.add("main_chart", "bga_columns", bga);
    shot.add("main_chart", "asset_columns", assetColumns);
    shot.add("main_chart", "density",
             QRect(editorOrigin + QPoint(editor->width() - Editor::kDensityWidth, 0), QSize(Editor::kDensityWidth, editor->height())));
    shot.region("main_chart", "minimap", &window, window.chartMinimap());
    shot.region("main_chart", "bga_panel", &window, window.bgaPanel());
    shot.region("main_chart", "statusbar", &window, window.statusBar());
    shot.add("main_chart", "header", QRect(editorOrigin, QSize(editor->width(), Editor::kHeaderHeight)));

    int menuIndex = 0;
    for (auto* action : window.menuBar()->actions()) {
        if (auto* menu = action->menu()) {
            menu->adjustSize();
            shot.save(menu, "menu_" + std::to_string(menuIndex++));
        }
    }

    {
        gui::SongSettingsDialog dialog(gui::SongSettingsDialog::Purpose::EditSong, controller.project(),
                                       gui::SongSettingsContext{controller.document().chartDirectory(), controller.modeHints(), 120.0}, &window);
        dialog.show();
        QApplication::processEvents();
        shot.save(&dialog, "song_info");
        if (auto* tabs = dialog.findChild<QTabWidget*>()) {
            tabs->setCurrentIndex(1);
            QApplication::processEvents();
            shot.save(&dialog, "song_info_images");
        }
    }
    {
        gui::SongSettingsDialog dialog(gui::SongSettingsDialog::Purpose::NewProject, core::Project{},
                                       gui::SongSettingsContext{{}, controller.modeHints(), std::nullopt}, &window);
        dialog.show();
        QApplication::processEvents();
        shot.save(&dialog, "new_chart");
    }
    {
        app::PlayerSettings player;
        player.executable = "C:\\Games\\Pulsus\\Pulsus.exe";
        gui::StudioSettingsDialog dialog(player, &window);
        dialog.show();
        QApplication::processEvents();
        shot.save(&dialog, "settings");
    }
    {
        gui::StatisticsDialog dialog(controller, &window);
        dialog.show();
        settle(controller);
        shot.save(&dialog, "statistics");
    }
    {
        const auto doubled = leadCuts.size() > 6 ? leadCuts[6] : leadCuts.front();
        controller.document().apply("二重", [&](core::Project& edited) { return edited.addNote(doubled, 6, 0); });
        gui::NullTestDialog dialog(controller, gui::NullTestSource::InternalPlayer, &window);
        dialog.show();
        settle(controller);
        shot.save(&dialog, "nulltest");
        controller.document().undo();
    }
    {
        gui::AboutDialog dialog(&window);
        dialog.show();
        QApplication::processEvents();
        shot.save(&dialog, "about");
    }
    shot.write();
    std::cout << "wrote " << infra::pathToUtf8(shot.directory) << "\n";
    return 0;
}
