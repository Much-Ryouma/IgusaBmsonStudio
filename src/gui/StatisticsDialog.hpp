#pragma once

#include <array>
#include <optional>

#include <QDialog>

#include "StudioController.hpp"

class QLabel;
class QTableWidget;

namespace ibs::gui {

class StatisticsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit StatisticsDialog(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] QTableWidget* table() const noexcept { return table_; }
    void showReport(const app::ChartStatisticsReport& report);

private:
    void addRow(const QString& label, const QStringList& values, const std::array<std::optional<double>, 3>& positions = {});
    [[nodiscard]] QString positionText(double seconds) const;

    StudioController& controller_;
    QLabel* status_ = nullptr;
    QTableWidget* table_ = nullptr;
};

}  // namespace ibs::gui
