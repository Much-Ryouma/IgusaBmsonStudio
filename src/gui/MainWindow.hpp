#pragma once

#include <filesystem>

#include <QMainWindow>

#include "StudioController.hpp"

class QAction;
class QComboBox;
class QLabel;

namespace ibs::gui {

class ArrangerWidget;
class SlicerPanel;
class WaveformSlicerWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] ArrangerWidget* arranger() const noexcept { return arranger_; }
    [[nodiscard]] WaveformSlicerWidget* slicer() const noexcept { return slicer_; }
    [[nodiscard]] SlicerPanel* slicerPanel() const noexcept { return slicerPanel_; }
    [[nodiscard]] QAction* undoAction() const noexcept { return undo_; }
    [[nodiscard]] QAction* redoAction() const noexcept { return redo_; }

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
    void showStatus(const QString& message);

    StudioController& controller_;
    ArrangerWidget* arranger_ = nullptr;
    WaveformSlicerWidget* slicer_ = nullptr;
    SlicerPanel* slicerPanel_ = nullptr;
    QAction* undo_ = nullptr;
    QAction* redo_ = nullptr;
    QComboBox* snap_ = nullptr;
};

}  // namespace ibs::gui
