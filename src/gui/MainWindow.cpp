#include "MainWindow.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>

#include "ArrangerWidget.hpp"
#include "QtText.hpp"
#include "SlicerPanel.hpp"
#include "SongSettingsDialog.hpp"
#include "WaveformSlicerWidget.hpp"
#include "ibs/core/BuildInfo.hpp"

namespace ibs::gui {

namespace {

constexpr int kStatusTimeoutMs = 8000;
constexpr int kMaximumListedIssues = 20;
constexpr int kSnapChoices[] = {4, 8, 12, 16, 24, 32, 48, 64};

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
    arranger_ = new ArrangerWidget(controller_, splitter);
    auto* bottom = new QWidget(splitter);
    auto* bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    slicer_ = new WaveformSlicerWidget(controller_, bottom);
    slicerPanel_ = new SlicerPanel(controller_, bottom);
    slicerPanel_->setFixedWidth(300);
    bottomLayout->addWidget(slicer_, 1);
    bottomLayout->addWidget(slicerPanel_);
    splitter->addWidget(arranger_);
    splitter->addWidget(bottom);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    buildMenus();
    buildToolBar();

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
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(QStringLiteral("ファイル(&F)"));
    file->addAction(QStringLiteral("新しい譜面…"), QKeySequence::New, this, &MainWindow::newChart);
    file->addAction(QStringLiteral("開く…"), QKeySequence::Open, this, &MainWindow::openChartDialog);
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

    auto* view = menuBar()->addMenu(QStringLiteral("表示(&V)"));
    view->addAction(QStringLiteral("曲全体を表示"), QKeySequence(QStringLiteral("Ctrl+0")), this, [this] { arranger_->zoomToFit(); });

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
                                                "  波形をクリック: そのスライスを試聴、←/→: 前後のスライスを試聴\n"
                                                "  ダブルクリック: 切断点を追加、切断点をドラッグ: 移動\n"
                                                "  Shift+ドラッグ: 範囲選択、Ctrl+クリック: 選択に追加\n"
                                                "  Delete: 削除、Q: グリッドに寄せる、P: 配置全体を試聴、F: 全体を表示\n\n"
                                                "線の色: 青=自動、橙=手動、灰=読み込み、赤=アタック欠け、点線=検出結果の候補"));
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
    SongSettingsDialog dialog(SongSettingsDialog::Purpose::NewProject, core::Project{}, this);
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
        QMessageBox::warning(this, QStringLiteral("開けませんでした"), error);
        return false;
    }
    arranger_->zoomToFit();
    refreshTitle();
    refreshUndoActions();
    return true;
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
    SongSettingsDialog dialog(SongSettingsDialog::Purpose::EditSong, controller_.project(), this);
    if (dialog.exec() == QDialog::Accepted) {
        const auto result = dialog.result();
        controller_.updateSongInfo(result.info, result.initialBpm);
    }
}

void MainWindow::showStatus(const QString& message) { statusBar()->showMessage(message, kStatusTimeoutMs); }

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard()) {
        controller_.stopPlayback();
        event->accept();
    } else {
        event->ignore();
    }
}

}  // namespace ibs::gui
