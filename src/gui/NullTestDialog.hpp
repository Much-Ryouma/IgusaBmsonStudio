#pragma once

#include <QDialog>

#include "StudioController.hpp"

class QLabel;
class QTableWidget;

namespace ibs::gui {

class NullTestDialog final : public QDialog {
    Q_OBJECT

public:
    NullTestDialog(StudioController& controller, NullTestSource source, QWidget* parent = nullptr);

    [[nodiscard]] QLabel* summary() const noexcept { return summary_; }
    [[nodiscard]] QTableWidget* table() const noexcept { return table_; }
    void showReport(const app::NullTestReport& report);

private:
    StudioController& controller_;
    QLabel* summary_ = nullptr;
    QTableWidget* table_ = nullptr;
};

}  // namespace ibs::gui
