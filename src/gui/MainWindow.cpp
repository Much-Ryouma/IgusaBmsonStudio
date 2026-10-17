#include "MainWindow.hpp"

#include <cmath>
#include <cstdint>

#include <QAction>
#include <QLineEdit>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>

#include "ArrangerWidget.hpp"
#include "BgaPanel.hpp"
#include "AboutDialog.hpp"
#include "NullTestDialog.hpp"
#include "StatisticsDialog.hpp"
#include "StudioSettingsDialog.hpp"
#include "TimelineScrollBar.hpp"
#include "ChartEditorWidget.hpp"
#include "ChartMinimap.hpp"
#include "QtText.hpp"
#include "StudioSettings.hpp"
#include "SlicerPanel.hpp"
#include "SongSettingsDialog.hpp"
#include "WaveformSlicerWidget.hpp"
#include "ibs/app/RecentFiles.hpp"
#include "ibs/core/BuildInfo.hpp"
#include "ibs/core/chart/ChartEditing.hpp"
#include "ibs/core/validation/Validator.hpp"

namespace ibs::gui {

namespace {

constexpr int kStatusTimeoutMs = 8000;
constexpr int kMaximumListedIssues = 20;
constexpr int kSnapChoices[] = {4, 8, 12, 16, 24, 32, 48, 64};
const QString kRecentFilesKey = QStringLiteral("file/recentFiles");
const QString kGeometryKey = QStringLiteral("window/geometry");
const QString kStateKey = QStringLiteral("window/state");
const QString kFollowPlaybackKey = QStringLiteral("view/followPlayback");

std::vector<std::string> loadRecentFiles() {
    std::vector<std::string> recent;
    for (const auto& entry : openStudioSettings()->value(kRecentFilesKey).toStringList()) {
        recent.push_back(toUtf8(QDir::toNativeSeparators(entry)));
    }
    return recent;
}

void storeRecentFiles(const std::vector<std::string>& recent) {
    QStringList entries;
    // Why not OS の区切り文字のまま書く: ini では \ が \\ と二重に書かれ、利用者が手で直すときに読みにくいため
    for (const auto& entry : recent) {
        entries << QDir::fromNativeSeparators(toQString(entry));
    }
    openStudioSettings()->setValue(kRecentFilesKey, entries);
}

QString issuesText(const std::vector<core::Issue>& issues, core::Severity severity) {
    QStringList lines;
    for (const auto& issue : issues) {
        if (issue.severity == severity && lines.size() < kMaximumListedIssues) {
            lines << QStringLiteral("[%1] %2").arg(toQString(issue.code), toQString(issue.message));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

}  // namespace

MainWindow::MainWindow(StudioController& controller, QWidget* parent) : QMainWindow(parent), controller_(controller) {
    resize(1400, 900);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    tabs_ = new QTabWidget(splitter);
    arranger_ = new ArrangerWidget(controller_, tabs_);
    auto* chartPage = new QWidget(tabs_);
    auto* chartLayout = new QHBoxLayout(chartPage);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    auto* chartColumn = new QWidget(chartPage);
    auto* chartColumnLayout = new QVBoxLayout(chartColumn);
    chartColumnLayout->setContentsMargins(0, 0, 0, 0);
    chartColumnLayout->setSpacing(0);
    chartEditor_ = new ChartEditorWidget(controller_, chartColumn);
    chartScroll_ = new QScrollBar(Qt::Horizontal, chartColumn);
    chartScroll_->setObjectName(QStringLiteral("chartHorizontalScroll"));
    chartScroll_->hide();
    chartColumnLayout->addWidget(chartEditor_, 1);
    chartColumnLayout->addWidget(chartScroll_);
    chartMinimap_ = new ChartMinimap(controller_, *chartEditor_, chartPage);
    bgaPanel_ = new BgaPanel(controller_, chartPage);
    bgaPanel_->setFixedWidth(240);
    bgaPanel_->setSaveRequest([this] { return saveChartAs(); });
    chartLayout->setSpacing(0);
    chartLayout->addWidget(chartColumn, 1);
    chartLayout->addWidget(chartMinimap_);
    chartLayout->addWidget(bgaPanel_);
    connect(chartScroll_, &QScrollBar::valueChanged, chartEditor_,
            [this](int value) { chartEditor_->setHorizontalOffset(static_cast<double>(value)); });
    // Why not 列の数が変わったときだけ合わせる: 窓の大きさ・BGM 列の折りたたみ・ホイールでも見える幅が変わるため、描くたびに合わせる
    chartEditor_->setViewObserver([this] {
        // Why not 毎回スクロールバーを設定し直す: 値が同じでもスクロールバーと周りの再描画が起き、譜面を描くたびに連鎖するため、変わったときだけ触る
        const int overflow = std::max(0, static_cast<int>(std::ceil(chartEditor_->columnsWidth() - chartEditor_->visibleColumnsWidth())));
        const int page = static_cast<int>(chartEditor_->visibleColumnsWidth());
        const int value = static_cast<int>(chartEditor_->horizontalOffset());
        if (chartScroll_->maximum() != overflow || chartScroll_->pageStep() != page || chartScroll_->value() != value) {
            const QSignalBlocker blocker(chartScroll_);
            chartScroll_->setRange(0, overflow);
            chartScroll_->setPageStep(page);
            chartScroll_->setSingleStep(20);
            chartScroll_->setValue(value);
        }
        if (chartScroll_->isHidden() == (overflow > 0)) {
            chartScroll_->setVisible(overflow > 0);
        }
        chartMinimap_->refreshView();
    });
    auto* arrangerPage = new QWidget(tabs_);
    auto* arrangerLayout = new QVBoxLayout(arrangerPage);
    arrangerLayout->setContentsMargins(0, 0, 0, 0);
    arrangerLayout->setSpacing(0);
    arranger_->setParent(arrangerPage);
    auto* arrangerScroll = new TimelineScrollBar(
        {&arranger_->viewport(), arranger_, [] { return 0.0; }, [this] { return arranger_->contentEndPulse(); },
         [this] { return arranger_->timelineWidthPixels(); }},
        arrangerPage);
    arranger_->setViewObserver([arrangerScroll] { arrangerScroll->sync(); });
    arrangerLayout->addWidget(arranger_, 1);
    arrangerLayout->addWidget(arrangerScroll);
    tabs_->addTab(arrangerPage, QStringLiteral("アレンジャー"));
    tabs_->addTab(chartPage, QStringLiteral("譜面"));
    auto* bottom = new QWidget(splitter);
    auto* bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    auto* slicerColumn = new QWidget(bottom);
    auto* slicerLayout = new QVBoxLayout(slicerColumn);
    slicerLayout->setContentsMargins(0, 0, 0, 0);
    slicerLayout->setSpacing(0);
    slicer_ = new WaveformSlicerWidget(controller_, slicerColumn);
    auto* slicerScroll = new TimelineScrollBar(
        {&slicer_->viewport(), slicer_, [this] { return slicer_->contentStartPulse(); },
         [this] { return slicer_->contentEndPulse(); }, [this] { return static_cast<double>(slicer_->width()); }},
        slicerColumn);
    slicer_->setViewObserver([slicerScroll] { slicerScroll->sync(); });
    slicerLayout->addWidget(slicer_, 1);
    slicerLayout->addWidget(slicerScroll);
    slicerPanel_ = new SlicerPanel(controller_, bottom);
    slicerPanel_->setFixedWidth(300);
    bottomLayout->addWidget(slicerColumn, 1);
    bottomLayout->addWidget(slicerPanel_);
    splitter->addWidget(tabs_);
    splitter->addWidget(bottom);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({540, 360});
    setCentralWidget(splitter);

    controller_.setPlayerSettings(StudioSettingsDialog::load());
    buildMenus();
    buildChartMenu();
    buildToolBar();
    chartStats_ = new QLabel(this);
    statusBar()->addPermanentWidget(chartStats_);
    connect(&controller_, &StudioController::tempoDesynced, this, &MainWindow::handleTempoDesync);
    connect(&controller_, &StudioController::documentChanged, this, &MainWindow::refreshChartStats);
    refreshChartStats();

    connect(arranger_, &ArrangerWidget::placementActivated, this, [this](core::PlacementId placement) {
        controller_.selectPlacement(placement);
        slicer_->setFocus();
    });
    connect(&controller_, &StudioController::documentChanged, this, [this] {
        refreshTitle();
        refreshUndoActions();
    });
    connect(&controller_, &StudioController::statusMessage, this, &MainWindow::showStatus);
    refreshTitle();
    refreshUndoActions();
    statusBar()->showMessage(QStringLiteral("v") + toQString(core::kVersion));
    const auto settings = openStudioSettings();
    restoreGeometry(settings->value(kGeometryKey).toByteArray());
    restoreState(settings->value(kStateKey).toByteArray());
    if (!studioSettingsWritable()) {
        statusBar()->showMessage(QStringLiteral("設定を保存できません（%1 に書き込めません）。書き込めるフォルダに展開してください")
                                     .arg(QDir::toNativeSeparators(studioSettingsFile())));
    }
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(QStringLiteral("ファイル(&F)"));
    file->addAction(QStringLiteral("新しい譜面…"), QKeySequence::New, this, &MainWindow::newChart);
    file->addAction(QStringLiteral("開く…"), QKeySequence::Open, this, &MainWindow::openChartDialog);
    recentMenu_ = file->addMenu(QStringLiteral("最近使った譜面"));
    connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshRecentMenu);
    refreshRecentMenu();
    file->addAction(QStringLiteral("保存"), QKeySequence::Save, this, [this] { saveChart(); });
    file->addAction(QStringLiteral("名前を付けて保存…"), QKeySequence::SaveAs, this, [this] { saveChartAs(); });
    file->addSeparator();
    file->addAction(QStringLiteral("素材を取り込む…"), QKeySequence(QStringLiteral("Ctrl+I")), this, &MainWindow::importAudio);
    file->addAction(QStringLiteral("曲の情報…"), this, &MainWindow::editSongInfo);
    file->addSeparator();
    file->addAction(QStringLiteral("終了"), QKeySequence::Quit, this, &QWidget::close);

    auto* edit = menuBar()->addMenu(QStringLiteral("編集(&E)"));
    undo_ = edit->addAction(QStringLiteral("元に戻す"), QKeySequence::Undo, this, [this] { controller_.document().undo(); });
    redo_ = edit->addAction(QStringLiteral("やり直す"), QKeySequence::Redo, this, [this] { controller_.document().redo(); });

    auto* play = menuBar()->addMenu(QStringLiteral("再生(&P)"));
    play->addAction(QStringLiteral("カーソルから曲を再生"), QKeySequence(Qt::Key_F5), this, [this] { controller_.playSongFromCursor(); });
    play->addAction(QStringLiteral("停止"), QKeySequence(Qt::Key_F6), this, [this] { controller_.stopPlayback(); });
    play->addSeparator();
    play->addAction(QStringLiteral("外部プレーヤーで再生（カーソルの小節から）"), QKeySequence(Qt::Key_F7), this, [this] {
        if (controller_.playerSettings().executable.empty()) {
            editSettings();
        }
        controller_.previewInPlayer();
    });
    play->addAction(QStringLiteral("設定…"), this, &MainWindow::editSettings);

    auto* view = menuBar()->addMenu(QStringLiteral("表示(&V)"));
    view->addAction(QStringLiteral("曲全体を表示"), QKeySequence(QStringLiteral("Ctrl+0")), this, [this] { arranger_->zoomToFit(); });
    auto* followPlayback = view->addAction(QStringLiteral("再生位置に合わせてスクロールする"));
    followPlayback->setCheckable(true);
    controller_.setFollowsPlayback(openStudioSettings()->value(kFollowPlaybackKey, true).toBool());
    followPlayback->setChecked(controller_.followsPlayback());
    connect(followPlayback, &QAction::toggled, this, [this](bool enabled) {
        controller_.setFollowsPlayback(enabled);
        openStudioSettings()->setValue(kFollowPlaybackKey, enabled);
    });

    auto* statistics = menuBar()->addMenu(QStringLiteral("統計(&S)"));
    statistics->addAction(QStringLiteral("譜面の統計…"), QKeySequence(QStringLiteral("Ctrl+Shift+I")), this, [this] {
        auto* dialog = new StatisticsDialog(controller_, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    statistics->addSeparator();
    statistics->addAction(QStringLiteral("ヌルテスト（内蔵の再生）…"), this, [this] {
        auto* dialog = new NullTestDialog(controller_, NullTestSource::InternalPlayer, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    statistics->addAction(QStringLiteral("ヌルテスト（プレーヤーで書き出し）…"), this, [this] {
        if (controller_.playerSettings().executable.empty()) {
            editSettings();
        }
        auto* dialog = new NullTestDialog(controller_, NullTestSource::ExternalPlayer, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });

    auto* help = menuBar()->addMenu(QStringLiteral("ヘルプ(&H)"));
    help->addAction(QStringLiteral("操作方法"), this, [this] {
        QMessageBox::information(this, QStringLiteral("操作方法"),
                                 QStringLiteral("アレンジャー\n"
                                                "  素材の行の空いた所をダブルクリック: 配置を追加\n"
                                                "  配置をドラッグ: 移動（Ctrl で複製、Shift で細かく、Alt でスナップなし）\n"
                                                "  配置をダブルクリック / Enter: 音切りの画面で開く、Delete: 削除\n"
                                                "  上の目盛りをクリック: 再生カーソル、Space: 曲の再生/停止\n"
                                                "  Ctrl+ホイール: 拡大縮小、Shift+ホイール: 横スクロール\n\n"
                                                "音切り\n"
                                                "  波形をクリック: そのスライスを試聴、←/→: 前後のスライスを試聴（画面の外なら自動でスクロール）\n"
                                                "  ダブルクリック: 切断点を追加、切断点をドラッグ: 移動\n"
                                                "  Shift+ドラッグ: 範囲選択、Ctrl+クリック: 選択に追加\n"
                                                "  Delete: 削除、Q: グリッドに寄せる、P: 配置全体を試聴、F: 全体を表示\n\n"
                                                "線の色: 青=自動、橙=手動、灰=読み込み、赤=アタック欠け、点線=検出結果の候補\n\n"
                                                "譜面（割当モード）\n"
                                                "  右の素材列の切断点をクリック: 選択して試聴、↑/↓: 次/前の切断点\n"
                                                "  1〜7: 鍵盤に割当、S: スクラッチ、0: 割当を外す（BGM に戻す）\n"
                                                "  ノーツを横にドラッグ: レーン変更（複数選択中はまとめて）、外へ出す: 割当を外す\n"
                                                "  Shift+ドラッグ: ロングノーツの長さ、Shift+ダブルクリック: ロングノーツ ⇔ 通常\n"
                                                "  空いた所をドラッグ: 範囲選択、クリック: カーソル、Ctrl+C / Ctrl+V: 割当パターン\n"
                                                "  右クリック: BPM・STOP・拍子・小節線、Tab: 切断点モード（ダブルクリックで追加、ドラッグで移動）\n"
                                                "  BGA / LYR / POOR 列: 右の一覧で画像を選び、ダブルクリックで置く。ドラッグで移動、Delete で削除\n\n"
                                                "F7: 外部プレーヤー（Pulsus など）でカーソルの小節から再生\n"
                                                "Ctrl+Shift+I: 譜面の統計、統計 → ヌルテスト: 配置どおりに鳴るかの確認"));
    });
    help->addAction(QStringLiteral("使い方（Wiki）を開く"), QKeySequence::HelpContents, this, [] {
        QDesktopServices::openUrl(QUrl(toQString(core::kHomepageUrl) + QStringLiteral("/wiki")));
    });
    help->addSeparator();
    help->addAction(QStringLiteral("バージョン情報…"), this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });
}

void MainWindow::buildToolBar() {
    auto* toolbar = addToolBar(QStringLiteral("ツールバー"));
    toolbar->setObjectName(QStringLiteral("mainToolBar"));
    toolbar->addAction(QStringLiteral("▶ 再生"), this, [this] { controller_.playSongFromCursor(); });
    toolbar->addAction(QStringLiteral("■ 停止"), this, [this] { controller_.stopPlayback(); });
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(QStringLiteral(" スナップ "), toolbar));
    snap_ = new QComboBox(toolbar);
    snap_->addItem(QStringLiteral("なし"), 0);
    for (const int divisions : kSnapChoices) {
        snap_->addItem(QStringLiteral("%1 分").arg(divisions), divisions);
    }
    snap_->setCurrentIndex(snap_->findData(controller_.snapDivisions()));
    connect(snap_, &QComboBox::currentIndexChanged, this, [this] {
        controller_.setSnapDivisions(snap_->currentData().toInt());
        slicer_->update();
    });
    toolbar->addWidget(snap_);
}

void MainWindow::refreshTitle() {
    const auto& path = controller_.document().path();
    const QString name = path ? toQString(path->filename()) : QStringLiteral("無題");
    setWindowTitle(QStringLiteral("%1%2 - %3").arg(name, controller_.document().isDirty() ? QStringLiteral("*") : QString(),
                                                    toQString(core::kApplicationName)));
}

void MainWindow::refreshUndoActions() {
    const auto& document = controller_.document();
    undo_->setEnabled(document.canUndo());
    redo_->setEnabled(document.canRedo());
    undo_->setText(document.canUndo() ? QStringLiteral("元に戻す: %1").arg(toQString(document.undoLabel())) : QStringLiteral("元に戻す"));
    redo_->setText(document.canRedo() ? QStringLiteral("やり直す: %1").arg(toQString(document.redoLabel())) : QStringLiteral("やり直す"));
}

bool MainWindow::confirmDiscard() {
    if (!controller_.document().isDirty()) {
        return true;
    }
    const auto answer = QMessageBox::question(this, QStringLiteral("未保存の変更"),
                                              QStringLiteral("保存していない変更があります。保存しますか？"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        return saveChart();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::newChart() {
    if (!confirmDiscard()) {
        return;
    }
    SongSettingsDialog dialog(SongSettingsDialog::Purpose::NewProject, core::Project{},
                              SongSettingsContext{{}, controller_.modeHints(), std::nullopt}, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    controller_.newProject(dialog.result());
    refreshTitle();
    refreshUndoActions();
}

bool MainWindow::openChart(const std::filesystem::path& path) {
    QString error;
    if (!controller_.open(path, &error)) {
        storeRecentFiles(app::withoutRecentFile(loadRecentFiles(), path));
        QMessageBox::warning(this, QStringLiteral("開けませんでした"), error);
        return false;
    }
    storeRecentFiles(app::withRecentFile(loadRecentFiles(), path));
    arranger_->zoomToFit();
    warnUnreachableBga();
    refreshTitle();
    refreshUndoActions();
    return true;
}

// Why not 保存のときの検証（V04）だけに任せる: 動画を別のフォルダに置いたまま作業を続けると、提出の直前まで
// BGA が抜けていることに気づかないため、開いた時点で知らせる
std::vector<std::string> MainWindow::warnUnreachableBga() {
    const auto unreachable = controller_.unreachableBgaFiles();
    if (unreachable.empty()) {
        return unreachable;
    }
    QStringList names;
    for (const auto& name : unreachable) {
        names << toQString(name);
    }
    QMessageBox::warning(this, QStringLiteral("BGA のファイルが見つかりません"),
                         QStringLiteral("次の BGA（画像・動画）は、譜面（bmson）と同じフォルダの中にありません。\n"
                                        "プレーヤーで表示されないので、譜面のフォルダへ移すか、BGA 一覧から追加し直してください。\n\n%1")
                             .arg(names.join(QLatin1Char('\n'))));
    return unreachable;
}

void MainWindow::openChartDialog() {
    if (!confirmDiscard()) {
        return;
    }
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("譜面を開く"), QString(),
                                                      QStringLiteral("bmson (*.bmson)"));
    if (!file.isEmpty()) {
        openChart(toPath(file));
    }
}

bool MainWindow::saveChart() {
    const auto& path = controller_.document().path();
    return path ? saveTo(*path) : saveChartAs();
}

bool MainWindow::saveChartAs() {
    const auto& current = controller_.document().path();
    const QString file = QFileDialog::getSaveFileName(this, QStringLiteral("名前を付けて保存"),
                                                      current ? toQString(*current) : QString(),
                                                      QStringLiteral("bmson (*.bmson)"));
    return !file.isEmpty() && saveTo(toPath(file));
}

bool MainWindow::saveTo(const std::filesystem::path& path) {
    const auto outcome = controller_.save(path);
    if (!outcome.saved) {
        const QString details = issuesText(outcome.issues, core::Severity::Error);
        QMessageBox::warning(this, QStringLiteral("保存できませんでした"),
                             toQString(outcome.error) + (details.isEmpty() ? QString() : QStringLiteral("\n\n") + details));
        return false;
    }
    const auto warnings = std::count_if(outcome.issues.begin(), outcome.issues.end(),
                                        [](const core::Issue& issue) { return issue.severity == core::Severity::Warning; });
    showStatus(warnings > 0 ? QStringLiteral("保存しました（警告 %1 件）").arg(warnings) : QStringLiteral("保存しました"));
    storeRecentFiles(app::withRecentFile(loadRecentFiles(), path));
    refreshTitle();
    return true;
}

void MainWindow::importAudio() {
    if (!controller_.document().path()) {
        QMessageBox::information(this, QStringLiteral("素材を取り込む"),
                                 QStringLiteral("素材は譜面からの相対パスで保存します。先に「新しい譜面」で保存先を決めてください。"));
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("素材を取り込む"),
                                                            toQString(controller_.document().chartDirectory()),
                                                            QStringLiteral("音声 (*.wav *.ogg)"));
    std::optional<app::OutsideFilePolicy> policy;
    QStringList errors;
    int added = 0;
    for (const QString& file : files) {
        const auto path = toPath(file);
        if (!app::assetNameInside(controller_.document().chartDirectory(), path) && !policy) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("譜面のフォルダの外の素材"),
                QStringLiteral("譜面のフォルダの外にある素材は、フォルダにコピーして使います。コピーしますか？"));
            policy = answer == QMessageBox::Yes ? app::OutsideFilePolicy::Copy : app::OutsideFilePolicy::Reject;
        }
        const auto outcome = controller_.importAudio(path, policy.value_or(app::OutsideFilePolicy::Reject));
        if (!outcome.error.empty()) {
            errors << toQString(outcome.error);
        } else if (!outcome.alreadyImported) {
            ++added;
        }
    }
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("取り込めなかった素材"), errors.join(QLatin1Char('\n')));
    }
    showStatus(QStringLiteral("%1 個の素材を追加しました。素材の行の空いた所をダブルクリックすると配置できます").arg(added));
}

void MainWindow::editSongInfo() {
    std::optional<double> suggested;
    if (const auto* mode = controller_.keyMode()) {
        suggested = core::recommendedTotal(core::countPlayableNotes(controller_.project(), *mode));
    }
    SongSettingsDialog dialog(SongSettingsDialog::Purpose::EditSong, controller_.project(),
                              SongSettingsContext{controller_.document().chartDirectory(), controller_.modeHints(), suggested}, this);
    if (dialog.exec() == QDialog::Accepted) {
        const auto result = dialog.result();
        controller_.updateSongInfo(result.info, result.initialBpm);
    }
}

void MainWindow::buildChartMenu() {
    auto* chart = new QMenu(QStringLiteral("譜面(&C)"), this);
    const auto actions = menuBar()->actions();
    menuBar()->insertMenu(actions.size() >= 2 ? actions.at(actions.size() - 2) : nullptr, chart);
    auto* cutMode = chart->addAction(QStringLiteral("切断点モード（Tab）"));
    cutMode->setCheckable(true);
    connect(cutMode, &QAction::toggled, this, [this](bool enabled) {
        chartEditor_->setMode(enabled ? ChartEditMode::CutPoints : ChartEditMode::Assign);
    });
    connect(chartEditor_, &ChartEditorWidget::modeChanged, this, [cutMode](ChartEditMode current) {
        const QSignalBlocker blocker(cutMode);
        cutMode->setChecked(current == ChartEditMode::CutPoints);
    });
    auto* advance = chart->addAction(QStringLiteral("割り当てたら次の切断点へ進む"));
    advance->setCheckable(true);
    advance->setChecked(chartEditor_->autoAdvance());
    connect(advance, &QAction::toggled, this, [this](bool enabled) { chartEditor_->setAutoAdvance(enabled); });
    auto* follow = chart->addAction(QStringLiteral("クリックしたスライスを波形に表示する"));
    follow->setCheckable(true);
    {
        const auto settings = openStudioSettings();
        controller_.setFollowsChartSelection(settings->value(QStringLiteral("view/followChartSelection"), true).toBool());
    }
    follow->setChecked(controller_.followsChartSelection());
    connect(follow, &QAction::toggled, this, [this](bool enabled) {
        controller_.setFollowsChartSelection(enabled);
        const auto settings = openStudioSettings();
        settings->setValue(QStringLiteral("view/followChartSelection"), enabled);
    });
    auto* collapse = chart->addAction(QStringLiteral("BGM 列を折りたたむ"));
    collapse->setCheckable(true);
    connect(collapse, &QAction::toggled, this, [this](bool collapsed) { chartEditor_->setBgmColumnsCollapsed(collapsed); });
    chart->addSeparator();
    chart->addAction(QStringLiteral("割当パターンをコピー"), this, [this] { controller_.copySelectedNotes(); });
    chart->addAction(QStringLiteral("カーソル位置に貼り付け"), this, [this] {
        try {
            controller_.pasteAtCursor();
        } catch (const std::exception& error) {
            showStatus(toQString(error.what()));
        }
    });
    chart->addSeparator();
    chart->addAction(QStringLiteral("ミラー"), this, [this] {
        if (const auto* mode = controller_.keyMode()) {
            transformSelected(QStringLiteral("ミラー"), core::mirroredLanes(controller_.project(), controller_.selectedNotes(), *mode));
        }
    });
    chart->addAction(QStringLiteral("レーンを入れ替える…"), this, [this] {
        bool ok = false;
        const QString text = QInputDialog::getText(this, QStringLiteral("レーンの入れ替え"),
                                                   QStringLiteral("入れ替える 2 つのレーンの x（例: 1 7）"), QLineEdit::Normal,
                                                   QStringLiteral("1 7"), &ok);
        const auto parts = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (ok && parts.size() == 2) {
            transformSelected(QStringLiteral("レーンの入れ替え"),
                              core::swappedLanes(controller_.project(), controller_.selectedNotes(), parts[0].toInt(), parts[1].toInt()));
        }
    });
    chart->addAction(QStringLiteral("ランダム…"), this, [this] {
        const auto* mode = controller_.keyMode();
        bool ok = false;
        const int seed = QInputDialog::getInt(this, QStringLiteral("ランダム"), QStringLiteral("シード（同じ値なら同じ並びになります）"),
                                              1, 0, 2147483647, 1, &ok);
        if (ok && mode != nullptr) {
            transformSelected(QStringLiteral("ランダム"), core::randomizedLanes(controller_.project(), controller_.selectedNotes(), *mode,
                                                                            static_cast<std::uint32_t>(seed)));
        }
    });
}

void MainWindow::transformSelected(const QString& label, const std::map<core::NoteId, int>& lanes) {
    try {
        const auto changed = app::remapLanes(controller_.document(), toUtf8(label), lanes);
        showStatus(QStringLiteral("%1: %2 個のノーツを動かしました").arg(label).arg(changed));
    } catch (const std::exception&) {
        showStatus(QStringLiteral("%1 できません: 同じスライスの同じレーンに重なるノーツがあります").arg(label));
    }
}

void MainWindow::refreshChartStats() {
    const auto* mode = controller_.keyMode();
    if (mode == nullptr) {
        chartStats_->setText(QStringLiteral("モード定義なし"));
        return;
    }
    const auto notes = core::countPlayableNotes(controller_.project(), *mode);
    chartStats_->setText(QStringLiteral("ノーツ %1 / total %2（目安 %3）")
                             .arg(notes)
                             .arg(controller_.project().info.total, 0, 'g', 6)
                             .arg(std::lround(core::recommendedTotal(notes))));
}

// Why not 自動で切断点を動かす: テンポを変えた理由が「音源もそのテンポで書き出し直す」場合は pulse のままが正しく、
// どちらが正しいかは作者にしか分からないため、毎回選んでもらう
void MainWindow::handleTempoDesync(const app::TimingEditOutcome& outcome) {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("テンポの変更で切断点がずれます"));
    box.setText(QStringLiteral("%1 個の配置で、切断点と音の立ち上がりがずれます。").arg(outcome.desynced.size()));
    box.setInformativeText(QStringLiteral("「時間を保つ」: 切断点を動かして、音のずれをなくします（音源は今のまま使う場合）。\n"
                                          "「そのまま」: 切断点の拍の位置を保ちます（音源を新しいテンポで書き出し直す場合）。\n"
                                          "閉じた場合は警告 (V12) が残り、後で選べます。"));
    auto* keep = box.addButton(QStringLiteral("時間を保つ"), QMessageBox::AcceptRole);
    auto* accept = box.addButton(QStringLiteral("そのまま"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("後で決める"), QMessageBox::RejectRole);
    box.exec();
    try {
        if (box.clickedButton() == keep) {
            const auto moved = app::keepCutTimes(controller_.document(), outcome.desynced, outcome.before);
            showStatus(QStringLiteral("%1 個の切断点を動かしました").arg(moved));
        } else if (box.clickedButton() == accept) {
            app::acceptTempoDrift(controller_.document(), outcome.desynced);
        }
    } catch (const std::exception& error) {
        showStatus(toQString(error.what()));
    }
}

void MainWindow::editSettings() {
    StudioSettingsDialog dialog(controller_.playerSettings(), this);
    if (dialog.exec() == QDialog::Accepted) {
        controller_.setPlayerSettings(dialog.result());
        StudioSettingsDialog::save(dialog.result());
    }
}

void MainWindow::showStatus(const QString& message) { statusBar()->showMessage(message, kStatusTimeoutMs); }

void MainWindow::refreshRecentMenu() {
    recentMenu_->clear();
    const auto recent = loadRecentFiles();
    for (const auto& entry : recent) {
        const auto path = toPath(toQString(entry));
        recentMenu_->addAction(toQString(entry), this, [this, path] {
            if (confirmDiscard()) {
                openChart(path);
            }
        });
    }
    recentMenu_->setEnabled(!recent.empty());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard()) {
        controller_.stopPlayback();
        const auto settings = openStudioSettings();
        settings->setValue(kGeometryKey, saveGeometry());
        settings->setValue(kStateKey, saveState());
        event->accept();
    } else {
        event->ignore();
    }
}

}  // namespace ibs::gui
