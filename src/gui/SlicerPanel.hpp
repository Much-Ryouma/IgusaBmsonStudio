#pragma once

#include <QWidget>

#include "StudioController.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

namespace ibs::gui {

class SlicerPanel final : public QWidget {
    Q_OBJECT

public:
    explicit SlicerPanel(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] app::SliceSettings settings() const;
    void setSettings(const app::SliceSettings& settings);

    [[nodiscard]] QPushButton* applyButton() const noexcept { return applyButton_; }
    [[nodiscard]] QSlider* sensitivitySlider() const noexcept { return sensitivity_; }
    [[nodiscard]] QLabel* statusLabel() const noexcept { return status_; }

private:
    void onEdited();
    void onApply();
    void refreshStatus();
    void refreshEnabled();

    StudioController& controller_;
    QComboBox* function_ = nullptr;
    QSlider* sensitivity_ = nullptr;
    QLabel* sensitivityValue_ = nullptr;
    QDoubleSpinBox* minimumInterval_ = nullptr;
    QDoubleSpinBox* silenceGate_ = nullptr;
    QDoubleSpinBox* preroll_ = nullptr;
    QComboBox* grid_ = nullptr;
    QSlider* strength_ = nullptr;
    QDoubleSpinBox* window_ = nullptr;
    QCheckBox* autoPreview_ = nullptr;
    QPushButton* previewButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QLabel* status_ = nullptr;
    bool updating_ = false;
};

}  // namespace ibs::gui
