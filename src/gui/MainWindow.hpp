#pragma once

#include <filesystem>
#include <map>

#include <QMainWindow>

#include "StudioController.hpp"

class QAction;
class QComboBox;
class QLabel;
class QMenu;
class QTabWidget;

namespace ibs::gui {

class ArrangerWidget;
class BgaPanel;
class ChartEditorWidget;
class SlicerPanel;
class WaveformSlicerWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] ArrangerWidget* arranger() const noexcept { return arranger_; }
    [[nodiscard]] WaveformSlicerWidget* slicer() const noexcept { return slicer_; }
    [[nodiscard]] ChartEditorWidget* chartEditor() const noexcept { return chartEditor_; }
    [[nodiscard]] QTabWidget* tabs() const noexcept { return tabs_; }
    [[nodiscard]] BgaPanel* bgaPanel() const noexcept { return bgaPanel_; }
    void handleTempoDesync(const app::TimingEditOutcome& outcome);
    [[nodiscard]] SlicerPanel* slicerPanel() const noexcept { return slicerPanel_; }
    [[nodiscard]] QAction* undoAction() const noexcept { return undo_; }
    [[nodiscard]] QAction* redoAction() const noexcept { return redo_; }
    [[nodiscard]] QMenu* recentMenu() const noexcept { return recentMenu_; }

    bool openChart(const std::filesystem::path& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildMenus();
    void buildToolBar();
    void refreshTitle();
    void refreshUndoActions();
    [[nodiscard]] bool confirmDiscard();
    void newChart();
    void openChartDialog();
    bool saveChart();
    bool saveChartAs();
    bool saveTo(const std::filesystem::path& path);
    void importAudio();
    void editSongInfo();
    void editSettings();
    void showStatus(const QString& message);
    void buildChartMenu();
    void refreshChartStats();
    void refreshRecentMenu();
    void transformSelected(const QString& label, const std::map<core::NoteId, int>& lanes);

    StudioController& controller_;
    ArrangerWidget* arranger_ = nullptr;
    WaveformSlicerWidget* slicer_ = nullptr;
    ChartEditorWidget* chartEditor_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    BgaPanel* bgaPanel_ = nullptr;
    QLabel* chartStats_ = nullptr;
    SlicerPanel* slicerPanel_ = nullptr;
    QAction* undo_ = nullptr;
    QAction* redo_ = nullptr;
    QComboBox* snap_ = nullptr;
    QMenu* recentMenu_ = nullptr;
};

}  // namespace ibs::gui
