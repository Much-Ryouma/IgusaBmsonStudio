#include "ArrangerWidget.hpp"

#include <algorithm>
#include <cmath>

#include <QContextMenuEvent>
#include <QLineEdit>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include "QtText.hpp"
#include "StudioPalette.hpp"
#include "TimelineMath.hpp"
#include "ibs/app/ArrangementCommands.hpp"
#include "ibs/app/ChartCommands.hpp"
#include "ibs/core/timing/TimingEdits.hpp"
#include "ibs/core/playback/PlaybackRange.hpp"
#include "ibs/core/timing/BarLines.hpp"

namespace ibs::gui {

namespace {

constexpr double kMinimumGridSpacingPixels = 10.0;
constexpr int kMuteButtonX = 104;
constexpr int kSoloButtonX = 126;
constexpr int kButtonSize = 18;
constexpr double kWheelZoomStep = 1.25;
constexpr int kUnknownLengthBars = 4;

}  // namespace

ArrangerWidget::ArrangerWidget(StudioController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumHeight(kRulerHeight + kRowHeight * 2);
    const auto refresh = [this] { update(); };
    connect(&controller_, &StudioController::documentChanged, this, refresh);
    connect(&controller_, &StudioController::selectionChanged, this, refresh);
    connect(&controller_, &StudioController::playheadChanged, this, refresh);
    connect(&controller_, &StudioController::cursorChanged, this, refresh);
    connect(&controller_, &StudioController::mixChanged, this, refresh);
}

double ArrangerWidget::timelineX(double pulse) const { return kHeaderWidth + viewport_.xOf(pulse); }

double ArrangerWidget::pulseAtX(double x) const { return viewport_.pulseAt(x - kHeaderWidth); }

int ArrangerWidget::rowTop(std::size_t row) const {
    return kRulerHeight + static_cast<int>(row) * kRowHeight - static_cast<int>(verticalOffset_);
}

std::optional<std::size_t> ArrangerWidget::rowAt(double y) const {
    if (y < kRulerHeight) {
        return std::nullopt;
    }
    const auto row = static_cast<std::size_t>((y - kRulerHeight + verticalOffset_) / kRowHeight);
    return row < controller_.project().assets().size() ? std::optional(row) : std::nullopt;
}

core::Pulse ArrangerWidget::placementEnd(const core::Placement& placement) const {
    const auto& project = controller_.project();
    const auto audio = controller_.library().cached(project.asset(placement.assetId).name);
    if (!audio) {
        return placement.startPulse + static_cast<core::Pulse>(project.timing.resolution) * 4 * kUnknownLengthBars;
    }
    return core::placementEndPulse(project, placement.id, audio->audio.durationSeconds());
}

QRectF ArrangerWidget::placementRect(core::PlacementId placementId) const {
    const auto& project = controller_.project();
    const auto& placement = project.placement(placementId);
    const auto& assets = project.assets();
    const auto row = static_cast<std::size_t>(std::distance(
        assets.begin(), std::find_if(assets.begin(), assets.end(), [&](const core::Asset& asset) { return asset.id == placement.assetId; })));
    const double left = timelineX(static_cast<double>(placement.startPulse));
    const double right = timelineX(static_cast<double>(placementEnd(placement)));
    return QRectF(left, rowTop(row) + 3.0, std::max(4.0, right - left), kRowHeight - 6.0);
}

std::optional<core::PlacementId> ArrangerWidget::placementAt(QPointF point) const {
    if (point.x() < kHeaderWidth) {
        return std::nullopt;
    }
    const auto& placements = controller_.project().placements();
    // Why not 先に追加した配置を優先する: 重なった配置では後から追加したものが上に描かれるため、見えている方をつかめるよう逆順に探す
    for (auto it = placements.rbegin(); it != placements.rend(); ++it) {
        if (placementRect(it->id).contains(point)) {
            return it->id;
        }
    }
    return std::nullopt;
}

void ArrangerWidget::zoomToFit() {
    const auto& project = controller_.project();
    core::Pulse end = static_cast<core::Pulse>(project.timing.resolution) * 16;
    for (const auto& placement : project.placements()) {
        end = std::max(end, placementEnd(placement));
    }
    viewport_.fit(0.0, static_cast<double>(end) * 1.05, std::max(100.0, static_cast<double>(width() - kHeaderWidth)));
    update();
}

void ArrangerWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette::kBackground);
    paintRows(painter);
    paintRuler(painter);

    painter.setPen(QPen(palette::kCursor, 1.0));
    const double cursorX = timelineX(static_cast<double>(controller_.cursorPulse()));
    if (cursorX >= kHeaderWidth) {
        painter.drawLine(QPointF(cursorX, 0), QPointF(cursorX, height()));
    }
    if (const auto playhead = controller_.playheadPulse()) {
        const double x = timelineX(*playhead);
        if (x >= kHeaderWidth) {
            painter.setPen(QPen(palette::kPlayhead, 1.5));
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
    }
}

void ArrangerWidget::paintRuler(QPainter& painter) {
    painter.fillRect(QRectF(0, 0, width(), kRulerHeight), palette::kRuler);
    const auto& timing = controller_.project().timing;
    const auto from = static_cast<core::Pulse>(std::floor(pulseAtX(kHeaderWidth)));
    const auto to = static_cast<core::Pulse>(std::ceil(pulseAtX(width())));
    painter.setPen(palette::kText);
    for (const auto& line : core::gridLinesBetween(timing, 1, std::max<core::Pulse>(0, from), to)) {
        const double x = timelineX(static_cast<double>(line.pulse));
        painter.drawLine(QPointF(x, kRulerHeight - 6), QPointF(x, kRulerHeight));
        painter.drawText(QPointF(x + 3, kRulerHeight - 7), QString::number(line.barIndex));
    }
    painter.fillRect(QRectF(0, 0, kHeaderWidth, kRulerHeight), palette::kRuler);
    painter.setPen(palette::kDimText);
    painter.drawText(QRectF(6, 0, kHeaderWidth - 6, kRulerHeight), Qt::AlignVCenter, QStringLiteral("素材 / 小節"));
}

void ArrangerWidget::paintRows(QPainter& painter) {
    const auto& project = controller_.project();
    const auto& timing = project.timing;
    const int divisions = viewport_.gridDivisionsFor(timing.resolution, kMinimumGridSpacingPixels, 4);
    const auto from = std::max<core::Pulse>(0, static_cast<core::Pulse>(std::floor(pulseAtX(kHeaderWidth))));
    const auto to = static_cast<core::Pulse>(std::ceil(pulseAtX(width())));

    for (std::size_t row = 0; row < project.assets().size(); ++row) {
        const int top = rowTop(row);
        if (top + kRowHeight < kRulerHeight || top > height()) {
            continue;
        }
        painter.fillRect(QRectF(kHeaderWidth, top, width() - kHeaderWidth, kRowHeight),
                         row % 2 == 0 ? palette::kBackground : palette::kRowAlternate);
    }
    for (const auto& line : core::gridLinesBetween(timing, divisions, from, to)) {
        const double x = timelineX(static_cast<double>(line.pulse));
        painter.setPen(line.kind == core::GridLineKind::Bar    ? palette::kBarLine
                       : line.kind == core::GridLineKind::Beat ? palette::kBeatLine
                                                               : palette::kSubdivisionLine);
        painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
    }

    for (const auto& placement : project.placements()) {
        const bool dragging = drag_ && drag_->placement == placement.id && drag_->moved;
        if (dragging && !drag_->duplicate) {
            continue;
        }
        paintPlacement(painter, placement, placement.startPulse, false);
    }
    if (drag_ && drag_->moved) {
        paintPlacement(painter, project.placement(drag_->placement), drag_->currentStart, true);
    }

    for (std::size_t row = 0; row < project.assets().size(); ++row) {
        const int top = rowTop(row);
        if (top + kRowHeight < kRulerHeight || top > height()) {
            continue;
        }
        const auto& asset = project.assets()[row];
        painter.fillRect(QRectF(0, top, kHeaderWidth, kRowHeight), palette::kRuler);
        painter.fillRect(QRectF(0, top, 5, kRowHeight), palette::assetColor(asset.id));
        const bool missing = !controller_.library().cached(asset.name);
        painter.setPen(missing ? palette::kDimText : palette::kText);
        painter.drawText(QRectF(10, top, kMuteButtonX - 12, kRowHeight), Qt::AlignVCenter | Qt::TextSingleLine,
                         painter.fontMetrics().elidedText(toQString(asset.name), Qt::ElideMiddle, kMuteButtonX - 12));
        const auto drawToggle = [&](int x, const QString& label, bool on, const QColor& color) {
            const QRectF box(x, top + (kRowHeight - kButtonSize) / 2.0, kButtonSize, kButtonSize);
            painter.fillRect(box, on ? color : palette::kBackground);
            painter.setPen(palette::kDimText);
            painter.drawRect(box);
            painter.setPen(on ? palette::kBackground : palette::kText);
            painter.drawText(box, Qt::AlignCenter, label);
        };
        drawToggle(kMuteButtonX, QStringLiteral("M"), controller_.mix().muted.contains(asset.id), palette::kManualCut);
        drawToggle(kSoloButtonX, QStringLiteral("S"), controller_.mix().soloed.contains(asset.id), palette::kAutoCut);
    }
    if (project.assets().empty()) {
        painter.setPen(palette::kDimText);
        painter.drawText(rect().adjusted(kHeaderWidth, kRulerHeight, 0, 0), Qt::AlignCenter,
                         QStringLiteral("ファイル → 素材を取り込む で、フレーズの音声を追加してください"));
    }
}

void ArrangerWidget::paintPlacement(QPainter& painter, const core::Placement& placement, core::Pulse start, bool ghost) {
    const auto& project = controller_.project();
    QRectF box = placementRect(placement.id);
    box.moveLeft(timelineX(static_cast<double>(start)));
    if (box.right() < kHeaderWidth || box.left() > width()) {
        return;
    }
    painter.save();
    painter.setClipRect(QRectF(kHeaderWidth, kRulerHeight, width() - kHeaderWidth, height() - kRulerHeight));
    QColor fill = palette::assetColor(placement.assetId);
    fill.setAlpha(ghost ? 90 : 200);
    painter.fillRect(box, fill);

    const auto audio = controller_.library().cached(project.asset(placement.assetId).name);
    if (audio && !ghost) {
        const auto timing = project.timing.toTimingMap();
        const double startSeconds = timing.secondsAt(start);
        const int rate = audio->audio.sampleRate;
        const double middle = box.center().y();
        const double halfHeight = box.height() / 2.0 - 2.0;
        painter.setPen(QColor(20, 20, 20, 170));
        const int firstX = static_cast<int>(std::max<double>(box.left(), kHeaderWidth));
        const int lastX = static_cast<int>(std::min<double>(box.right(), width()));
        for (int x = firstX; x < lastX; ++x) {
            const auto begin = static_cast<core::SampleIndex>((secondsAtFractionalPulse(timing, pulseAtX(x)) - startSeconds) * rate);
            const auto end = static_cast<core::SampleIndex>((secondsAtFractionalPulse(timing, pulseAtX(x + 1)) - startSeconds) * rate);
            const auto [low, high] = audio->waveform.range(begin, std::max(end, begin + 1));
            painter.drawLine(QPointF(x, middle - high * halfHeight), QPointF(x, middle - low * halfHeight));
        }
    }

    painter.setPen(QPen(QColor(255, 255, 255, 110), 1.0));
    for (const auto id : project.cutPointsOf(placement.id)) {
        const double x = timelineX(static_cast<double>(start + project.cutPoint(id).offsetPulses));
        painter.drawLine(QPointF(x, box.top()), QPointF(x, box.top() + 6));
    }
    const bool desynced = project.tempoAffectedPlacements().contains(placement.id);
    if (desynced) {
        painter.setPen(palette::kClippedCut);
        painter.drawText(box.adjusted(4, 1, -2, -2), Qt::AlignBottom | Qt::AlignLeft, QStringLiteral("テンポ変更でずれあり (V12)"));
    }
    const bool selected = controller_.selectedPlacement() == placement.id;
    painter.setPen(QPen(selected ? palette::kSelection : fill.darker(160), selected ? 2.0 : 1.0));
    painter.drawRect(box);
    painter.setPen(palette::kText);
    painter.drawText(box.adjusted(4, 1, -2, 0), Qt::AlignTop | Qt::AlignLeft,
                     QStringLiteral("y=%1").arg(start));
    painter.restore();
}

void ArrangerWidget::reportError(const std::exception& error) {
    emit controller_.statusMessage(toQString(error.what()));
}

void ArrangerWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    const QPointF point = event->position();
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (point.y() < kRulerHeight && point.x() >= kHeaderWidth) {
        controller_.setCursorPulse(controller_.snap(pulseAtX(point.x()), StudioController::snapModeFor(event->modifiers())));
        return;
    }
    if (point.x() < kHeaderWidth) {
        const auto row = rowAt(point.y());
        if (!row) {
            return;
        }
        const auto asset = controller_.project().assets()[*row].id;
        if (point.x() >= kMuteButtonX && point.x() < kMuteButtonX + kButtonSize) {
            controller_.toggleMute(asset);
        } else if (point.x() >= kSoloButtonX && point.x() < kSoloButtonX + kButtonSize) {
            controller_.toggleSolo(asset);
        }
        return;
    }
    const auto placement = placementAt(point);
    controller_.selectPlacement(placement);
    if (placement) {
        const auto start = controller_.project().placement(*placement).startPulse;
        drag_ = Drag{*placement, pulseAtX(point.x()) - static_cast<double>(start), start, start,
                     event->modifiers().testFlag(Qt::ControlModifier), false};
    }
}

void ArrangerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!drag_) {
        return;
    }
    const double pulse = pulseAtX(event->position().x()) - drag_->grabOffsetPulses;
    drag_->currentStart = controller_.snap(pulse, StudioController::snapModeFor(event->modifiers()));
    drag_->moved = drag_->moved || drag_->currentStart != drag_->originalStart;
    update();
}

void ArrangerWidget::mouseReleaseEvent(QMouseEvent*) {
    if (!drag_) {
        return;
    }
    const Drag finished = *drag_;
    drag_.reset();
    try {
        if (finished.moved && finished.duplicate) {
            controller_.selectPlacement(app::duplicatePlacement(controller_.document(), finished.placement, finished.currentStart));
        } else if (finished.moved) {
            app::movePlacement(controller_.document(), finished.placement, finished.currentStart);
        }
    } catch (const std::exception& error) {
        reportError(error);
    }
    update();
}

void ArrangerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    const QPointF point = event->position();
    if (point.x() < kHeaderWidth || point.y() < kRulerHeight) {
        return;
    }
    if (const auto placement = placementAt(point)) {
        emit placementActivated(*placement);
        return;
    }
    const auto row = rowAt(point.y());
    if (!row) {
        return;
    }
    try {
        const auto asset = controller_.project().assets()[*row].id;
        const auto start = controller_.snap(pulseAtX(point.x()), StudioController::snapModeFor(event->modifiers()));
        controller_.selectPlacement(app::placeAsset(controller_.document(), asset, start));
    } catch (const std::exception& error) {
        reportError(error);
    }
}

void ArrangerWidget::wheelEvent(QWheelEvent* event) {
    const QPoint angle = event->angleDelta();
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const double steps = angle.y() / 120.0;
        viewport_.zoomAround(event->position().x() - kHeaderWidth, std::pow(kWheelZoomStep, steps));
    } else if (event->modifiers().testFlag(Qt::ShiftModifier) || angle.x() != 0) {
        const int delta = angle.x() != 0 ? angle.x() : angle.y();
        viewport_.scrollByPixels(-delta / 2.0);
    } else {
        const double content = static_cast<double>(controller_.project().assets().size()) * kRowHeight;
        const double visible = height() - kRulerHeight;
        verticalOffset_ = std::clamp(verticalOffset_ - angle.y() / 2.0, 0.0, std::max(0.0, content - visible));
    }
    update();
    event->accept();
}

void ArrangerWidget::keyPressEvent(QKeyEvent* event) {
    const auto selected = controller_.selectedPlacement();
    try {
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && selected) {
            app::removePlacement(controller_.document(), *selected);
            return;
        }
        if (event->key() == Qt::Key_Space) {
            if (controller_.isPlaying()) {
                controller_.stopPlayback();
            } else {
                controller_.playSongFromCursor();
            }
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && selected) {
            emit placementActivated(*selected);
            return;
        }
    } catch (const std::exception& error) {
        reportError(error);
        return;
    }
    QWidget::keyPressEvent(event);
}

void ArrangerWidget::contextMenuEvent(QContextMenuEvent* event) {
    const auto placement = placementAt(event->pos());
    if (!placement) {
        return;
    }
    controller_.selectPlacement(placement);
    QMenu menu(this);
    menu.addAction(QStringLiteral("位置を数値で指定…"), this, [this, placement] {
        const auto& timing = controller_.project().timing;
        const auto start = controller_.project().placement(*placement).startPulse;
        bool ok = false;
        const QString text = QInputDialog::getText(
            this, QStringLiteral("配置の位置"),
            QStringLiteral("小節:拍（例 12:1。小節は 0 から、拍は 1 から）または pulse（例 11520）"), QLineEdit::Normal,
            QStringLiteral("%1:%2").arg(core::barIndexAt(timing, start))
                .arg((start - core::barStartPulse(timing, core::barIndexAt(timing, start))) / std::max(1, timing.resolution) + 1),
            &ok);
        if (!ok) {
            return;
        }
        core::Pulse target = -1;
        const auto parts = text.split(QLatin1Char(':'));
        if (parts.size() == 2) {
            target = core::barStartPulse(timing, parts[0].toInt()) +
                     static_cast<core::Pulse>(std::max(0, parts[1].toInt() - 1)) * timing.resolution;
        } else if (parts.size() == 1) {
            target = parts[0].toLongLong(&ok);
        }
        try {
            if (target < 0 || !ok) {
                throw std::invalid_argument("位置は 12:1 のように小節:拍で、または pulse の数値で入力してください");
            }
            app::movePlacement(controller_.document(), *placement, target);
        } catch (const std::exception& error) {
            reportError(error);
        }
    });
    menu.addAction(QStringLiteral("音切りの画面で開く"), this, [this, placement] { emit placementActivated(*placement); });
    menu.addAction(QStringLiteral("配置全体を試聴"), this, [this, placement] { controller_.playPlacement(*placement); });
    menu.addAction(QStringLiteral("削除"), this, [this, placement] {
        try {
            app::removePlacement(controller_.document(), *placement);
        } catch (const std::exception& error) {
            reportError(error);
        }
    });
    if (controller_.project().tempoAffectedPlacements().contains(*placement)) {
        menu.addSeparator();
        menu.addAction(QStringLiteral("テンポ変更のずれ: 切断点はこのままにする（警告を消す）"), this, [this, placement] {
            app::acceptTempoDrift(controller_.document(), {*placement});
        });
    }
    menu.exec(event->globalPos());
}

}  // namespace ibs::gui
