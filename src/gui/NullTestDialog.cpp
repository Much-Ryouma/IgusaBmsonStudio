#include "NullTestDialog.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "QtText.hpp"

namespace ibs::gui {

namespace {

constexpr int kPulseRole = Qt::UserRole + 1;

QString clock(double seconds) {
    const int minutes = static_cast<int>(seconds / 60.0);
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds - minutes * 60.0, 4, 'f', 1, QLatin1Char('0'));
}

}  // namespace

NullTestDialog::NullTestDialog(StudioController& controller, NullTestSource source, QWidget* parent)
    : QDialog(parent), controller_(controller) {
    setWindowTitle(source == NullTestSource::InternalPlayer ? QStringLiteral("ヌルテスト（内蔵の再生）")
                                                            : QStringLiteral("ヌルテスト（プレーヤーで書き出し）"));
    resize(560, 440);
    auto* layout = new QVBoxLayout(this);
    summary_ = new QLabel(source == NullTestSource::InternalPlayer
                              ? QStringLiteral("比較中…（配置どおりの音と、書き出す bmson の鳴り方を比べています）")
                              : QStringLiteral("プレーヤーで書き出しています…"),
                          this);
    summary_->setWordWrap(true);
    summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(summary_);
    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({QStringLiteral("時刻"), QStringLiteral("小節"), QStringLiteral("差（dB）")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_, 1);
    layout->addWidget(new QLabel(QStringLiteral("行をダブルクリックすると、カーソルがその区間へ移ります"), this));
    auto* close = new QPushButton(QStringLiteral("閉じる"), this);
    layout->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (const auto* item = table_->item(row, 0); item != nullptr) {
            controller_.setCursorPulse(item->data(kPulseRole).toLongLong());
        }
    });
    connect(&controller_, &StudioController::nullTestReady, this, &NullTestDialog::showReport);
    connect(&controller_, &StudioController::nullTestFailed, this,
            [this](const QString& reason) { summary_->setText(QStringLiteral("比較できませんでした: %1").arg(reason)); });
    controller_.requestNullTest(source);
}

void NullTestDialog::showReport(const app::NullTestReport& report) {
    QString text = report.passed ? QStringLiteral("一致: 差は %1 dB で、配置どおりに鳴ります。")
                                 : QStringLiteral("不一致: 差は %1 dB です。下の区間で鳴り方が配置と違います。");
    text = text.arg(report.residualDb, 0, 'f', 1);
    text += QStringLiteral("\nずれ %1 ms、音量の倍率 %2").arg(report.offsetMs, 0, 'f', 2).arg(report.gain, 0, 'f', 3);
    if (!report.missingAssets.empty()) {
        QStringList names;
        for (const auto& name : report.missingAssets) {
            names << toQString(name);
        }
        text += QStringLiteral("\n見つからない素材（比較から除外）: %1").arg(names.join(QStringLiteral("、")));
    }
    summary_->setText(text);
    table_->setRowCount(0);
    for (const auto& section : report.worst) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto* time = new QTableWidgetItem(clock(section.startSeconds));
        time->setData(kPulseRole, static_cast<qlonglong>(section.pulse));
        table_->setItem(row, 0, time);
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(section.bar)));
        table_->setItem(row, 2, new QTableWidgetItem(QString::number(section.residualDb, 'f', 1)));
    }
}

}  // namespace ibs::gui
