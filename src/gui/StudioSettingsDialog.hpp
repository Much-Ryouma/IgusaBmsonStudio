#pragma once

#include <QDialog>

#include "ibs/app/PreviewService.hpp"

class QCheckBox;
class QLineEdit;

namespace ibs::gui {

class StudioSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    StudioSettingsDialog(const app::PlayerSettings& current, QWidget* parent = nullptr);

    [[nodiscard]] app::PlayerSettings result() const;

    static app::PlayerSettings load();
    static void save(const app::PlayerSettings& settings);

private:
    QLineEdit* executable_ = nullptr;
    QLineEdit* commandTemplate_ = nullptr;
    QCheckBox* foldBgmColumns_ = nullptr;
    QLineEdit* renderTemplate_ = nullptr;
};

}  // namespace ibs::gui
