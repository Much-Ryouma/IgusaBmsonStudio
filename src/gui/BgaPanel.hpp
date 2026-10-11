#pragma once

#include <QWidget>

#include "StudioController.hpp"

class QListWidget;
class QPushButton;

namespace ibs::gui {

class BgaPanel final : public QWidget {
    Q_OBJECT

public:
    explicit BgaPanel(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] QListWidget* list() const noexcept { return list_; }
    bool addImage(const std::filesystem::path& file);

private:
    void refresh();
    void browse();
    void removeSelected();

    StudioController& controller_;
    QListWidget* list_ = nullptr;
    QPushButton* remove_ = nullptr;
};

}  // namespace ibs::gui
