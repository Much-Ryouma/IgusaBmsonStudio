#include "MainWindow.hpp"

#include <QLabel>
#include <QStatusBar>
#include <QString>

#include "ibs/core/BuildInfo.hpp"

namespace ibs::gui {

namespace {

QString fromView(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(fromView(ibs::core::kApplicationName));
    resize(1280, 800);

    auto* placeholder = new QLabel(tr("No project is open."), this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);

    statusBar()->showMessage(QStringLiteral("v") + fromView(ibs::core::kVersion));
}

}  // namespace ibs::gui
