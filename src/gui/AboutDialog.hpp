#pragma once

#include <QDialog>

class QLabel;
class QTextBrowser;

namespace ibs::gui {

class AboutDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    [[nodiscard]] QLabel* iconLabel() const noexcept { return icon_; }
    [[nodiscard]] QLabel* versionLabel() const noexcept { return version_; }
    [[nodiscard]] QTextBrowser* notices() const noexcept { return notices_; }

private:
    QLabel* icon_ = nullptr;
    QLabel* version_ = nullptr;
    QTextBrowser* notices_ = nullptr;
};

}  // namespace ibs::gui
