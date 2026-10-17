#pragma once

#include <functional>
#include <vector>

#include <QWidget>

#include "StudioController.hpp"

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QListWidget;
class QPushButton;

namespace ibs::gui {

class BgaPanel final : public QWidget {
    Q_OBJECT

public:
    explicit BgaPanel(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] QListWidget* list() const noexcept { return list_; }
    bool addImage(const std::filesystem::path& file);
    std::size_t addFiles(const std::vector<std::filesystem::path>& files);
    void setSaveRequest(std::function<bool()> saveChartAs) { saveChartAs_ = std::move(saveChartAs); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void refresh();
    void browse();
    void removeSelected();

    StudioController& controller_;
    QListWidget* list_ = nullptr;
    QPushButton* remove_ = nullptr;
    std::function<bool()> saveChartAs_;
};

}  // namespace ibs::gui
