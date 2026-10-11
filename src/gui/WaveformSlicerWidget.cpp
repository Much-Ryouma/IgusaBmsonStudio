#include "WaveformSlicerWidget.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "QtText.hpp"
#include "StudioPalette.hpp"
#include "TimelineMath.hpp"
#include "ibs/app/ArrangementCommands.hpp"
#include "ibs/core/playback/PlaybackRange.hpp"
#include "ibs/core/timing/BarLines.hpp"

namespace ibs::gui {

namespace {

constexpr double kMinimumGridSpacingPixels = 8.0;
constexpr double kFitMarginFraction = 0.02;
constexpr double kWheelZoomStep = 1.25;
constexpr int kDefaultQuantizeDivisions = 16;

}  // namespace

WaveformSlicerWidget::WaveformSlicerWidget(StudioController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(140);
    connect(&controller_, &StudioController::selectionChanged, this, &WaveformSlicerWidget::onSelectionChanged);
    connect(&controller_, &StudioController::documentChanged, this, [this] { update(); });
    connect(&controller_, &StudioController::previewChanged, this, [this] { update(); });
    connect(&controller_, &StudioController::playheadChanged, this, [this] { update(); });
    connect(&controller_, &StudioController::audioLoaded, this, [this] {
        if (shownPlacement_ && needsFit_) {
            fitPlacement();
        }
    });
    connect(&controller_, &StudioController::cutFocusRequested, this, [this](core::CutPointId cut) {
        if (shownPlacement_ && controller_.project().containsCutPoint(cut) &&
            controller_.project().cutPoint(cut).placementId == *shownPlacement_) {
            revealCut(cut, true);
        }
    });
}

void WaveformSlicerWidget::onSelectionChanged() {
    if (controller_.selectedPlacement() != shownPlacement_) {
        shownPlacement_ = controller_.selectedPlacement();
        drag_.reset();
        rubberBand_.reset();
        needsFit_ = true;
        fitPlacement();
    }
    update();
}

core::Pulse WaveformSlicerWidget::placementStart() const {
    return controller_.project().placement(*shownPlacement_).startPulse;
}

core::Pulse WaveformSlicerWidget::placementEnd() const {
    const auto& project = controller_.project();
    const auto& placement = project.placement(*shownPlacement_);
    const auto audio = controller_.library().cached(project.asset(placement.assetId).name);
    if (!audio) {
        return placement.startPulse + static_cast<core::Pulse>(project.timing.resolution) * 16;
    }
    return core::placementEndPulse(project, placement.id, audio->audio.durationSeconds());
}

void WaveformSlicerWidget::fitPlacement() {
    if (!shownPlacement_) {
        update();
        return;
    }
    const auto start = static_cast<double>(placementStart());
    const auto end = static_cast<double>(placementEnd());
    const double margin = (end - start) * kFitMarginFraction;
    viewport_.fit(std::max(0.0, start - margin), end + margin, std::max(50.0, static_cast<double>(width())));
    needsFit_ = width() < 50 || !controller_.library().cached(
                                    controller_.project().asset(controller_.project().placement(*shownPlacement_).assetId).name);
    update();
}

double WaveformSlicerWidget::contentStartPulse() const {
    return shownPlacement_ ? static_cast<double>(placementStart()) : 0.0;
}

double WaveformSlicerWidget::contentEndPulse() const {
    return shownPlacement_ ? static_cast<double>(placementEnd()) : 0.0;
}

// Why not 常に配置全体を表示し直す: 数分のステムでは 1 つのスライスが数画素になり、聴いている音の波形が見えないため、
// 必要なときだけ拡大し、それ以外は今の倍率のまま見える位置へ動かす
void WaveformSlicerWidget::revealRange(double fromPulse, double toPulse, bool zoomToRange) {
    const double visible = std::max(1.0, width() * viewport_.pulsesPerPixel());
    const double span = std::max(1.0, toPulse - fromPulse);
    const double resolution = static_cast<double>(std::max(1, controller_.project().timing.resolution));
    if (zoomToRange || span > visible * 0.9) {
        const double padding = std::max(span * 0.25, resolution);
        const double width = std::max(span + padding * 2.0, resolution * 4.0);
        const double center = (fromPulse + toPulse) / 2.0;
        viewport_.fit(std::max(0.0, center - width / 2.0), center + width / 2.0, std::max(50.0, static_cast<double>(this->width())));
    } else if (fromPulse < viewport_.originPulse() || toPulse > viewport_.originPulse() + visible) {
        viewport_.setOrigin(fromPulse - (visible - span) * 0.3);
    }
    update();
}

void WaveformSlicerWidget::revealCut(core::CutPointId cut, bool zoomToSlice) {
    const auto& project = controller_.project();
    const auto ordered = project.cutPointsOf(project.cutPoint(cut).placementId);
    const auto position = std::find(ordered.begin(), ordered.end(), cut);
    const auto from = static_cast<double>(project.absolutePulseOf(project.cutPoint(cut)));
    double to = static_cast<double>(placementEnd());
    if (position != ordered.end() && std::next(position) != ordered.end()) {
        to = static_cast<double>(project.absolutePulseOf(project.cutPoint(*std::next(position))));
    }
    revealRange(from, to, zoomToSlice);
}

void WaveformSlicerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (needsFit_ && !drag_) {
        fitPlacement();
    }
}

std::optional<core::CutPointId> WaveformSlicerWidget::cutNear(double x) const {
    if (!shownPlacement_) {
        return std::nullopt;
    }
    const auto& project = controller_.project();
    std::optional<core::CutPointId> nearest;
    double best = kGrabDistancePixels;
    for (const auto id : project.cutPointsOf(*shownPlacement_)) {
        const double distance = std::abs(viewport_.xOf(static_cast<double>(project.absolutePulseOf(project.cutPoint(id)))) - x);
        if (distance <= best) {
            best = distance;
            nearest = id;
        }
    }
    return nearest;
}

std::optional<core::CutPointId> WaveformSlicerWidget::sliceAt(double songPulse) const {
    if (!shownPlacement_) {
        return std::nullopt;
    }
    const auto& project = controller_.project();
    std::optional<core::CutPointId> found;
    for (const auto id : project.cutPointsOf(*shownPlacement_)) {
        if (static_cast<double>(project.absolutePulseOf(project.cutPoint(id))) <= songPulse) {
            found = id;
        }
    }
    return found;
}

void WaveformSlicerWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette::kBackground);
    if (!shownPlacement_) {
        painter.setPen(palette::kDimText);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("アレンジャーで配置を選ぶと、ここで音切りできます"));
        return;
    }
    paintGrid(painter);
    paintWaveform(painter);
    paintPreview(painter);
    paintCuts(painter);
    if (rubberBand_) {
        const double left = std::min(rubberBand_->fromX, rubberBand_->toX);
        const double right = std::max(rubberBand_->fromX, rubberBand_->toX);
        painter.fillRect(QRectF(left, kRulerHeight, right - left, height() - kRulerHeight), QColor(255, 255, 255, 40));
    }
    if (const auto playhead = controller_.playheadPulse()) {
        painter.setPen(QPen(palette::kPlayhead, 1.5));
        const double x = viewport_.xOf(*playhead);
        painter.drawLine(QPointF(x, 0), QPointF(x, height()));
    }
    if (viewObserver_) {
        viewObserver_();
    }
}

void WaveformSlicerWidget::paintGrid(QPainter& painter) {
    const auto& timing = controller_.project().timing;
    painter.fillRect(QRectF(0, 0, width(), kRulerHeight), palette::kRuler);
    const int finest = controller_.snapDivisions() > 0 ? controller_.snapDivisions() : kDefaultQuantizeDivisions;
    const int divisions = viewport_.gridDivisionsFor(timing.resolution, kMinimumGridSpacingPixels, finest);
    const auto from = std::max<core::Pulse>(0, static_cast<core::Pulse>(std::floor(viewport_.pulseAt(0))));
    const auto to = static_cast<core::Pulse>(std::ceil(viewport_.pulseAt(width())));
    for (const auto& line : core::gridLinesBetween(timing, divisions, from, to)) {
        const double x = viewport_.xOf(static_cast<double>(line.pulse));
        painter.setPen(line.kind == core::GridLineKind::Bar    ? palette::kBarLine
                       : line.kind == core::GridLineKind::Beat ? palette::kBeatLine
                                                               : palette::kSubdivisionLine);
        painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
        if (line.kind == core::GridLineKind::Bar) {
            painter.setPen(palette::kText);
            painter.drawText(QPointF(x + 3, kRulerHeight - 6), QString::number(line.barIndex));
        }
    }
}

void WaveformSlicerWidget::paintWaveform(QPainter& painter) {
    const auto& project = controller_.project();
    const auto& placement = project.placement(*shownPlacement_);
    const double startX = viewport_.xOf(static_cast<double>(placement.startPulse));
    const double endX = viewport_.xOf(static_cast<double>(placementEnd()));
    painter.fillRect(QRectF(0, kRulerHeight, std::max(0.0, startX), height()), QColor(0, 0, 0, 90));
    painter.fillRect(QRectF(endX, kRulerHeight, width() - endX, height()), QColor(0, 0, 0, 90));

    const auto lookup = controller_.library().cached(project.asset(placement.assetId).name);
    if (!lookup) {
        painter.setPen(palette::kDimText);
        painter.drawText(rect().adjusted(0, kRulerHeight, 0, 0), Qt::AlignCenter, QStringLiteral("音声を読み込み中…"));
        return;
    }
    const auto timing = project.timing.toTimingMap();
    const double startSeconds = timing.secondsAt(placement.startPulse);
    const int rate = lookup->audio.sampleRate;
    const double middle = (kRulerHeight + height()) / 2.0;
    const double halfHeight = (height() - kRulerHeight) / 2.0 - 4.0;
    painter.setPen(palette::kWaveform);
    const int firstX = std::max(0, static_cast<int>(startX));
    const int lastX = std::min(width(), static_cast<int>(std::ceil(endX)));
    for (int x = firstX; x < lastX; ++x) {
        const auto begin = static_cast<core::SampleIndex>((secondsAtFractionalPulse(timing, viewport_.pulseAt(x)) - startSeconds) * rate);
        const auto end = static_cast<core::SampleIndex>((secondsAtFractionalPulse(timing, viewport_.pulseAt(x + 1)) - startSeconds) * rate);
        const auto [low, high] = lookup->waveform.range(begin, std::max(end, begin + 1));
        painter.drawLine(QPointF(x, middle - high * halfHeight), QPointF(x, middle - low * halfHeight));
    }
}

void WaveformSlicerWidget::paintPreview(QPainter& painter) {
    const auto& preview = controller_.preview();
    if (!preview || preview->placementId != *shownPlacement_) {
        return;
    }
    const auto& project = controller_.project();
    std::set<core::Pulse> existing;
    for (const auto id : project.cutPointsOf(*shownPlacement_)) {
        existing.insert(project.cutPoint(id).offsetPulses);
    }
    for (const auto& planned : preview->plan.cuts) {
        if (existing.contains(planned.offsetPulses)) {
            continue;
        }
        const double x = viewport_.xOf(static_cast<double>(preview->placementStart + planned.offsetPulses));
        painter.setPen(QPen(planned.attackClipped ? palette::kClippedCut : palette::kAutoCut, 1.0, Qt::DashLine));
        painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
    }
}

void WaveformSlicerWidget::paintCuts(QPainter& painter) {
    const auto& project = controller_.project();
    const auto& selected = controller_.selectedCuts();
    const auto& preview = controller_.preview();
    const auto ordered = project.cutPointsOf(*shownPlacement_);
    const double endX = viewport_.xOf(static_cast<double>(placementEnd()));
    // Why not 常に同じ濃さで描く: 縮小表示では切断点の線が隙間なく並び、波形が見えなくなるため、線が密なときは薄くする
    const double startX = viewport_.xOf(static_cast<double>(placementStart()));
    const bool denseCuts = !ordered.empty() && (endX - startX) / static_cast<double>(ordered.size()) < 6.0;
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        const auto id = ordered[index];
        const auto& cut = project.cutPoint(id);
        const double x = viewport_.xOf(static_cast<double>(project.absolutePulseOf(cut)));
        const bool isSelected = std::find(selected.begin(), selected.end(), id) != selected.end();
        if (isSelected) {
            const double nextX = index + 1 < ordered.size()
                                     ? viewport_.xOf(static_cast<double>(project.absolutePulseOf(project.cutPoint(ordered[index + 1]))))
                                     : endX;
            painter.fillRect(QRectF(x, kRulerHeight, nextX - x, height() - kRulerHeight), QColor(255, 255, 255, 28));
        }
        const bool replaced = preview && std::find(preview->replaced.begin(), preview->replaced.end(), id) != preview->replaced.end();
        QColor color = cut.offsetPulses == 0 ? palette::kSelection : palette::cutColor(cut.origin, controller_.slicing().isAttackClipped(id));
        if (replaced) {
            color.setAlpha(80);
        } else if (!isSelected && denseCuts) {
            color.setAlpha(140);
        }
        painter.setPen(QPen(color, isSelected ? 3.0 : 1.0, replaced ? Qt::DotLine : Qt::SolidLine));
        painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
        if (project.hasNotes(id)) {
            painter.fillRect(QRectF(x - 3, kRulerHeight + 2, 6, 6), color);
        }
        if (isSelected) {
            const QPointF tip(x, kRulerHeight);
            const QPolygonF handle({tip, QPointF(x - 5, kRulerHeight - 8), QPointF(x + 5, kRulerHeight - 8)});
            painter.setBrush(color);
            painter.drawPolygon(handle);
            painter.setBrush(Qt::NoBrush);
        }
    }
    if (drag_ && drag_->moved) {
        const double x = viewport_.xOf(static_cast<double>(drag_->currentPulse));
        painter.setPen(QPen(palette::kManualCut, 2.0, Qt::DashLine));
        painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
    }
}

void WaveformSlicerWidget::reportError(const std::exception& error) {
    emit controller_.statusMessage(toQString(error.what()));
}

void WaveformSlicerWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (!shownPlacement_ || event->button() != Qt::LeftButton) {
        return;
    }
    const double x = event->position().x();
    const auto modifiers = event->modifiers();
    if (const auto near = cutNear(x)) {
        auto selection = controller_.selectedCuts();
        const auto found = std::find(selection.begin(), selection.end(), *near);
        if (modifiers.testFlag(Qt::ControlModifier)) {
            if (found != selection.end()) {
                selection.erase(found);
            } else {
                selection.push_back(*near);
            }
        } else if (found == selection.end()) {
            selection = {*near};
        }
        controller_.setSelectedCuts(selection);
        const auto& project = controller_.project();
        if (project.cutPoint(*near).offsetPulses > 0 && !modifiers.testFlag(Qt::ControlModifier)) {
            drag_ = CutDrag{*near, project.absolutePulseOf(project.cutPoint(*near)), false};
        }
        return;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        rubberBand_ = RubberBand{x, x};
        return;
    }
    if (const auto slice = sliceAt(viewport_.pulseAt(x))) {
        controller_.setSelectedCuts({*slice});
        controller_.playSlice(*slice);
    }
}

void WaveformSlicerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (drag_) {
        const auto pulse = controller_.snap(viewport_.pulseAt(event->position().x()), StudioController::snapModeFor(event->modifiers()));
        drag_->currentPulse = std::max(pulse, placementStart() + 1);
        drag_->moved = true;
        update();
    } else if (rubberBand_) {
        rubberBand_->toX = event->position().x();
        update();
    }
}

void WaveformSlicerWidget::mouseReleaseEvent(QMouseEvent*) {
    if (drag_) {
        const CutDrag finished = *drag_;
        drag_.reset();
        if (finished.moved) {
            try {
                app::moveCut(controller_.document(), finished.cut, finished.currentPulse);
            } catch (const std::exception& error) {
                reportError(error);
            }
        }
        update();
        return;
    }
    if (rubberBand_) {
        const double left = std::min(rubberBand_->fromX, rubberBand_->toX);
        const double right = std::max(rubberBand_->fromX, rubberBand_->toX);
        rubberBand_.reset();
        const auto& project = controller_.project();
        std::vector<core::CutPointId> selection;
        for (const auto id : project.cutPointsOf(*shownPlacement_)) {
            const double x = viewport_.xOf(static_cast<double>(project.absolutePulseOf(project.cutPoint(id))));
            if (x >= left && x <= right) {
                selection.push_back(id);
            }
        }
        controller_.setSelectedCuts(selection);
        update();
    }
}

void WaveformSlicerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!shownPlacement_ || event->button() != Qt::LeftButton || cutNear(event->position().x())) {
        return;
    }
    try {
        const auto pulse = controller_.snap(viewport_.pulseAt(event->position().x()), StudioController::snapModeFor(event->modifiers()));
        const auto cut = app::addManualCut(controller_.document(), *shownPlacement_, pulse);
        controller_.setSelectedCuts({cut});
    } catch (const std::exception& error) {
        reportError(error);
    }
}

void WaveformSlicerWidget::wheelEvent(QWheelEvent* event) {
    const QPoint angle = event->angleDelta();
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        viewport_.zoomAround(event->position().x(), std::pow(kWheelZoomStep, angle.y() / 120.0));
    } else {
        const int delta = angle.x() != 0 ? angle.x() : angle.y();
        viewport_.scrollByPixels(-delta / 2.0);
    }
    update();
    event->accept();
}

void WaveformSlicerWidget::stepSlice(int direction) {
    const auto& project = controller_.project();
    const auto ordered = project.cutPointsOf(*shownPlacement_);
    if (ordered.empty()) {
        return;
    }
    std::ptrdiff_t index = 0;
    if (!controller_.selectedCuts().empty()) {
        const auto found = std::find(ordered.begin(), ordered.end(), controller_.selectedCuts().front());
        index = found == ordered.end() ? 0 : std::distance(ordered.begin(), found) + direction;
    }
    index = std::clamp<std::ptrdiff_t>(index, 0, static_cast<std::ptrdiff_t>(ordered.size()) - 1);
    const auto cut = ordered[static_cast<std::size_t>(index)];
    controller_.setSelectedCuts({cut});
    revealCut(cut, false);
    controller_.playSlice(cut);
}

void WaveformSlicerWidget::keyPressEvent(QKeyEvent* event) {
    if (!shownPlacement_) {
        QWidget::keyPressEvent(event);
        return;
    }
    try {
        switch (event->key()) {
            case Qt::Key_Left: stepSlice(-1); return;
            case Qt::Key_Right: stepSlice(1); return;
            case Qt::Key_Space:
                if (controller_.isPlaying()) {
                    controller_.stopPlayback();
                } else if (!controller_.selectedCuts().empty()) {
                    controller_.playSlice(controller_.selectedCuts().front());
                } else {
                    controller_.playPlacement(*shownPlacement_);
                }
                return;
            case Qt::Key_P: controller_.playPlacement(*shownPlacement_); return;
            case Qt::Key_F: fitPlacement(); return;
            case Qt::Key_Delete:
            case Qt::Key_Backspace: {
                const auto requested = controller_.selectedCuts().size();
                const auto removed = app::removeCuts(controller_.document(), controller_.selectedCuts());
                if (removed < requested) {
                    emit controller_.statusMessage(
                        QStringLiteral("先頭とノーツのある切断点 %1 個は消していません").arg(requested - removed));
                }
                return;
            }
            case Qt::Key_Q: {
                const int divisions = controller_.snapDivisions() > 0 ? controller_.snapDivisions() : kDefaultQuantizeDivisions;
                const auto moved = app::snapCutsToGrid(controller_.document(), controller_.selectedCuts(), divisions);
                emit controller_.statusMessage(QStringLiteral("%1 個の切断点をグリッドに寄せました").arg(moved));
                return;
            }
            default: break;
        }
    } catch (const std::exception& error) {
        reportError(error);
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace ibs::gui
