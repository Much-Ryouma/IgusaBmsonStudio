#pragma once

#include <vector>

#include <QDialog>

#include "ibs/core/model/Project.hpp"

class QListWidget;
class QPushButton;

namespace ibs::gui {

class CutLayoutCopyDialog final : public QDialog {
    Q_OBJECT

public:
    CutLayoutCopyDialog(const core::Project& project, core::AssetId source, QWidget* parent = nullptr);

    [[nodiscard]] QListWidget* list() const noexcept { return list_; }
    [[nodiscard]] QPushButton* okButton() const noexcept { return ok_; }
    [[nodiscard]] std::vector<core::AssetId> chosenTargets() const;

private:
    QListWidget* list_ = nullptr;
    QPushButton* ok_ = nullptr;
};

}  // namespace ibs::gui
