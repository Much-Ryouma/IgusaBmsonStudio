#pragma once

#include <filesystem>
#include <optional>

#include <QDialog>

#include "StudioController.hpp"

class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace ibs::gui {

class SongSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Purpose { NewProject, EditSong };

    SongSettingsDialog(Purpose purpose, const core::Project& current, QWidget* parent = nullptr);

    [[nodiscard]] NewProjectSettings result() const;

private:
    void browse();
    void validate();

    Purpose purpose_;
    core::SongInfo baseInfo_;
    QLineEdit* path_ = nullptr;
    QLineEdit* title_ = nullptr;
    QLineEdit* subtitle_ = nullptr;
    QLineEdit* artist_ = nullptr;
    QLineEdit* genre_ = nullptr;
    QLineEdit* chartName_ = nullptr;
    QSpinBox* level_ = nullptr;
    QDoubleSpinBox* bpm_ = nullptr;
    QSpinBox* resolution_ = nullptr;
};

}  // namespace ibs::gui
