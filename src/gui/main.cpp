#include <QApplication>

#include "MainWindow.hpp"

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IgusaBmsonStudio"));

    ibs::gui::MainWindow window;
    window.show();

    return QApplication::exec();
}
