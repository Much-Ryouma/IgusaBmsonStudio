#pragma once

#include <QMainWindow>

namespace ibs::gui {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace ibs::gui
