#include "StatisticsDialog.hpp"

#include <array>
#include <cmath>
#include <optional>

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "QtText.hpp"
#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::gui {

namespace {

constexpr int kPulseRole = Qt::UserRole + 1;

QString dash() { return QStringLiteral("—"); }

QString decibels(double value) { return value <= -119.9 ? QStringLiteral("無音") : QStringLiteral("%1 dBFS").arg(value, 0, 'f', 1); }

QString clock(double seconds) {
    const int minutes = static_cast<int>(seconds / 60.0);
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds - minutes * 60.0, 4, 'f', 1, QLatin1Char('0'));
}

}  // namespace

StatisticsDialog::StatisticsDialog(StudioController& controller, QWidget* parent) : QDialog(parent), controller_(controller) {
    setWindowTitle(QStringLiteral("統計"));
    resize(760, 560);
    auto* layout = new QVBoxLayout(this);
    status_ = new QLabel(QStringLiteral("計算中…（素材を鳴らして音量を測っています）"), this);
    layout->addWidget(status_);
    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({QStringLiteral("項目"), QStringLiteral("譜面（鍵盤）"), QStringLiteral("BGM"), QStringLiteral("合計")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);
    auto* hint = new QLabel(QStringLiteral("位置のあるセルをダブルクリックすると、カーソルがそこへ移ります"), this);
    layout->addWidget(hint);
    auto* close = new QPushButton(QStringLiteral("閉じる"), this);
    layout->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        const auto* item = table_->item(row, column);
        if (item != nullptr && item->data(kPulseRole).isValid()) {
            controller_.setCursorPulse(item->data(kPulseRole).toLongLong());
        }
    });
    connect(&controller_, &StudioController::statisticsReady, this, &StatisticsDialog::showReport);
    connect(&controller_, &StudioController::statisticsFailed, this, [this](const QString& reason) { status_->setText(reason); });
    controller_.requestStatistics();
}

QString StatisticsDialog::positionText(double seconds) const {
    const auto& timing = controller_.project().timing;
    const auto pulse = static_cast<core::Pulse>(timing.toTimingMap().pulseAt(seconds));
    return QStringLiteral("%1・小節 %2").arg(clock(seconds)).arg(core::barIndexAt(timing, pulse));
}

void StatisticsDialog::addRow(const QString& label, const QStringList& values, const std::array<std::optional<double>, 3>& positions) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(label));
    for (int column = 0; column < 3; ++column) {
        auto* item = new QTableWidgetItem(column < values.size() ? values[column] : dash());
        if (positions[static_cast<std::size_t>(column)]) {
            const double seconds = *positions[static_cast<std::size_t>(column)];
            item->setText(item->text() + QStringLiteral("（%1）").arg(positionText(seconds)));
            item->setData(kPulseRole, static_cast<qlonglong>(controller_.project().timing.toTimingMap().pulseAt(seconds)));
        }
        table_->setItem(row, column + 1, item);
    }
}

void StatisticsDialog::showReport(const app::ChartStatisticsReport& report) {
    table_->setRowCount(0);
    status_->setText(report.missingAssets.empty()
                         ? QStringLiteral("音量は、それぞれの部分のスライスだけを鳴らして測りました")
                         : QStringLiteral("素材が見つからないため音量に含めていないもの: %1").arg(report.missingAssets.size()));
    const auto& notes = report.notes;
    const std::size_t total = notes.total + report.bgm.slices;
    addRow(QStringLiteral("ノーツ数（BGM はスライス数）"),
           {QString::number(notes.total), QString::number(report.bgm.slices), QString::number(total)});
    addRow(QStringLiteral("通常ノーツ"), {QString::number(notes.normal), dash(), QString::number(notes.normal)});
    addRow(QStringLiteral("ロングノーツ"), {QString::number(notes.longNotes), dash(), QString::number(notes.longNotes)});
    addRow(QStringLiteral("スクラッチ"), {QString::number(notes.scratch), dash(), QString::number(notes.scratch)});
    addRow(QStringLiteral("スクラッチのロングノーツ"), {QString::number(notes.scratchLong), dash(), QString::number(notes.scratchLong)});
    addRow(QStringLiteral("BGM 列のノーツ"), {dash(), QString::number(report.bgm.bgmColumnNotes), QString::number(report.bgm.bgmColumnNotes)});
    QStringList lanes;
    for (const auto& [lane, count] : notes.perLane) {
        lanes << QStringLiteral("%1: %2").arg(lane).arg(count);
    }
    addRow(QStringLiteral("レーンごと（x: 数）"), {lanes.join(QStringLiteral("  ")), dash(), dash()});

    const auto densityValue = [](const core::DensityStatistics& density) {
        return density.events == 0 ? dash() : QStringLiteral("%1 個/秒").arg(density.maxPerSecond);
    };
    const auto densityPosition = [](const core::DensityStatistics& density) {
        return density.events == 0 ? std::optional<double>{} : std::optional<double>{density.maxAtSeconds};
    };
    addRow(QStringLiteral("最大密度（1 秒）"),
           {densityValue(report.chartDensity), densityValue(report.bgmDensity), densityValue(report.totalDensity)},
           {densityPosition(report.chartDensity), densityPosition(report.bgmDensity), densityPosition(report.totalDensity)});
    const auto average = [](const core::DensityStatistics& density) {
        return density.events == 0 ? dash() : QStringLiteral("%1 個/秒").arg(density.averagePerSecond, 0, 'f', 2);
    };
    addRow(QStringLiteral("平均密度（最初〜最後のノーツ）"),
           {average(report.chartDensity), average(report.bgmDensity), average(report.totalDensity)});
    const auto span = [](const core::DensityStatistics& density) {
        return density.events == 0 ? dash() : clock(density.lastSeconds - density.firstSeconds);
    };
    addRow(QStringLiteral("演奏時間（最初〜最後のノーツ）"),
           {span(report.chartDensity), span(report.bgmDensity), span(report.totalDensity)});
    addRow(QStringLiteral("再生時間（音が鳴り終わるまで）"),
           {clock(report.chartLevel.durationSeconds), clock(report.bgmLevel.durationSeconds), clock(report.totalLevel.durationSeconds)});
    addRow(QStringLiteral("平均音量（実効値）"),
           {decibels(report.chartLevel.averageDb), decibels(report.bgmLevel.averageDb), decibels(report.totalLevel.averageDb)});
    const auto at = [](const core::LevelStatistics& level, double seconds) {
        return level.peakDb <= -119.9 ? std::optional<double>{} : std::optional<double>{seconds};
    };
    addRow(QStringLiteral("最大音量（ピーク）"),
           {decibels(report.chartLevel.peakDb), decibels(report.bgmLevel.peakDb), decibels(report.totalLevel.peakDb)},
           {at(report.chartLevel, report.chartLevel.peakAtSeconds), at(report.bgmLevel, report.bgmLevel.peakAtSeconds),
            at(report.totalLevel, report.totalLevel.peakAtSeconds)});
    addRow(QStringLiteral("いちばん大きい 1 秒（実効値）"),
           {decibels(report.chartLevel.loudestSecondDb), decibels(report.bgmLevel.loudestSecondDb), decibels(report.totalLevel.loudestSecondDb)},
           {at(report.chartLevel, report.chartLevel.loudestSecondAtSeconds), at(report.bgmLevel, report.bgmLevel.loudestSecondAtSeconds),
            at(report.totalLevel, report.totalLevel.loudestSecondAtSeconds)});
    table_->resizeRowsToContents();
}

}  // namespace ibs::gui
