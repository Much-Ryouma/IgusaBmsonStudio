#include "ChartEditorWidget.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>

#include <QContextMenuEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QWheelEvent>

#include "QtText.hpp"
#include "StudioPalette.hpp"
#include "ibs/app/ArrangementCommands.hpp"
#include "ibs/app/ChartCommands.hpp"
#include "ibs/core/chart/ChartEditing.hpp"
#include "ibs/core/timing/BarLines.hpp"
#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::gui {

namespace {

constexpr double kGroupGap = 8.0;
constexpr double kMinimumGridSpacingPixels = 8.0;
constexpr double kGrabDistancePixels = 4.0;
constexpr double kDragThresholdPixels = 3.0;
constexpr double kWheelZoomStep = 1.25;
constexpr int kFallbackLaneCount = 8;

QColor laneBackground(core::LaneKind kind) {
    switch (kind) {
        case core::LaneKind::White: return QColor(0x26, 0x29, 0x2f);
        case core::LaneKind::Black: return QColor(0x1d, 0x22, 0x2e);
        case core::LaneKind::Scratch: return QColor(0x2e, 0x22, 0x22);
        case core::LaneKind::Other: return QColor(0x24, 0x24, 0x24);
    }
    return QColor(0x24, 0x24, 0x24);
}

}  // namespace

ChartEditorWidget::ChartEditorWidget(StudioController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    const auto refresh = [this] { update(); };
    connect(&controller_, &StudioController::documentChanged, this, refresh);
    connect(&controller_, &StudioController::selectionChanged, this, refresh);
    connect(&controller_, &StudioController::playheadChanged, this, refresh);
    connect(&controller_, &StudioController::cursorChanged, this, refresh);
}

void ChartEditorWidget::setMode(ChartEditMode mode) {
    if (mode_ != mode) {
        mode_ = mode;
        emit modeChanged(mode);
        update();
    }
}

void ChartEditorWidget::setBgmColumnsCollapsed(bool collapsed) {
    bgmCollapsed_ = collapsed;
    update();
}

double ChartEditorWidget::timelineHeight() const { return height() - kHeaderHeight; }

double ChartEditorWidget::yOf(double pulse) const { return height() - viewport_.xOf(pulse); }

double ChartEditorWidget::pulseAtY(double y) const { return viewport_.pulseAt(height() - y); }

void ChartEditorWidget::scrollToPulse(double pulse) {
    const double y = yOf(pulse);
    if (y < kHeaderHeight + 20 || y > height() - 20) {
        viewport_.setOrigin(pulse - timelineHeight() * 0.3 * viewport_.pulsesPerPixel());
        update();
    }
}

int ChartEditorWidget::scratchLane() const {
    if (const auto* mode = controller_.keyMode()) {
        for (const auto& lane : mode->lanes()) {
            if (lane.kind == core::LaneKind::Scratch) {
                return lane.x;
            }
        }
    }
    return -1;
}

std::vector<int> ChartEditorWidget::bgmColumnLanes() const {
    const auto* mode = controller_.keyMode();
    const int start = mode != nullptr ? mode->bgmColumnStartX() : kFallbackLaneCount + 1;
    int last = start;
    for (const auto& note : controller_.project().notes()) {
        if (note.lane >= start) {
            last = std::max(last, note.lane + 1);
        }
    }
    std::vector<int> lanes;
    for (int x = start; x <= last; ++x) {
        lanes.push_back(x);
    }
    return lanes;
}

std::vector<ChartEditorWidget::Column> ChartEditorWidget::columns() const {
    std::vector<Column> result;
    double left = kRulerWidth + kTimingWidth - horizontalOffset_;
    if (const auto* mode = controller_.keyMode()) {
        for (const auto& lane : mode->lanes()) {
            const double width = kLaneUnitWidth * lane.width;
            result.push_back(Column{Column::Kind::Lane, lane.x, {}, left, width});
            left += width;
        }
    } else {
        for (int x = 1; x <= kFallbackLaneCount; ++x) {
            result.push_back(Column{Column::Kind::Lane, x, {}, left, kLaneUnitWidth});
            left += kLaneUnitWidth;
        }
    }
    left += kGroupGap;
    if (!bgmCollapsed_) {
        for (const int x : bgmColumnLanes()) {
            result.push_back(Column{Column::Kind::BgmColumn, x, {}, left, kBgmColumnWidth});
            left += kBgmColumnWidth;
        }
        left += kGroupGap;
    }
    for (const auto& asset : controller_.project().assets()) {
        result.push_back(Column{Column::Kind::Asset, 0, asset.id, left, kAssetColumnWidth});
        left += kAssetColumnWidth;
    }
    return result;
}

std::optional<ChartEditorWidget::Column> ChartEditorWidget::laneColumn(int x) const {
    for (const auto& column : columns()) {
        if (column.kind != Column::Kind::Asset && column.x == x) {
            return column;
        }
    }
    return std::nullopt;
}

std::optional<ChartEditorWidget::Column> ChartEditorWidget::assetColumn(core::AssetId asset) const {
    for (const auto& column : columns()) {
        if (column.kind == Column::Kind::Asset && column.asset == asset) {
            return column;
        }
    }
    return std::nullopt;
}

std::optional<ChartEditorWidget::Column> ChartEditorWidget::columnAt(double x) const {
    if (x < kRulerWidth + kTimingWidth || x > width() - kDensityWidth) {
        return std::nullopt;
    }
    for (const auto& column : columns()) {
        if (x >= column.left && x < column.left + column.width) {
            return column;
        }
    }
    return std::nullopt;
}

std::optional<core::NoteId> ChartEditorWidget::noteAt(QPointF point, bool* onLengthHandle) const {
    const auto column = columnAt(point.x());
    if (!column || column->kind == Column::Kind::Asset) {
        return std::nullopt;
    }
    const auto& project = controller_.project();
    for (const auto& note : project.notes()) {
        if (note.lane != column->x) {
            continue;
        }
        const double head = yOf(static_cast<double>(project.absolutePulseOf(project.cutPoint(note.cutPointId))));
        const double top = note.lengthPulses > 0
                               ? yOf(static_cast<double>(project.absolutePulseOf(project.cutPoint(note.cutPointId)) + note.lengthPulses))
                               : head - kNoteHeight;
        if (point.y() >= top - kGrabDistancePixels && point.y() <= head + 1.0) {
            if (onLengthHandle != nullptr) {
                *onLengthHandle = point.y() <= top + 2.0;
            }
            return note.id;
        }
    }
    return std::nullopt;
}

std::optional<core::CutPointId> ChartEditorWidget::cutMarkAt(QPointF point) const {
    const auto column = columnAt(point.x());
    if (!column || column->kind != Column::Kind::Asset) {
        return std::nullopt;
    }
    const auto& project = controller_.project();
    std::optional<core::CutPointId> nearest;
    double best = kGrabDistancePixels;
    for (const auto& cut : project.cutPoints()) {
        if (project.placement(cut.placementId).assetId != column->asset) {
            continue;
        }
        const double distance = std::abs(yOf(static_cast<double>(project.absolutePulseOf(cut))) - point.y());
        if (distance <= best) {
            best = distance;
            nearest = cut.id;
        }
    }
    return nearest;
}

void ChartEditorWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette::kBackground);
    const auto cols = columns();
    painter.save();
    painter.setClipRect(QRectF(0, kHeaderHeight, width() - kDensityWidth, height() - kHeaderHeight));
    paintGrid(painter, cols);
    paintTiming(painter);
    paintCutMarks(painter, cols);
    paintNotes(painter, cols);

    const double cursorY = yOf(static_cast<double>(controller_.cursorPulse()));
    painter.setPen(QPen(palette::kCursor, 1.0));
    painter.drawLine(QPointF(kRulerWidth, cursorY), QPointF(width() - kDensityWidth, cursorY));
    if (const auto playhead = controller_.playheadPulse()) {
        painter.setPen(QPen(palette::kPlayhead, 1.5));
        const double y = yOf(*playhead);
        painter.drawLine(QPointF(kRulerWidth, y), QPointF(width() - kDensityWidth, y));
    }
    if (drag_.kind == DragKind::RubberBand && drag_.moved) {
        painter.fillRect(QRectF(drag_.origin, drag_.current).normalized(), QColor(255, 255, 255, 40));
    }
    painter.restore();
    paintDensity(painter);
    paintHeader(painter, cols);
}

void ChartEditorWidget::paintGrid(QPainter& painter, const std::vector<Column>& cols) {
    for (const auto& column : cols) {
        QColor background = palette::kBackground;
        if (column.kind == Column::Kind::Lane) {
            const auto* mode = controller_.keyMode();
            core::LaneKind kind = core::LaneKind::White;
            if (mode != nullptr) {
                for (const auto& lane : mode->lanes()) {
                    kind = lane.x == column.x ? lane.kind : kind;
                }
            }
            background = laneBackground(kind);
        } else if (column.kind == Column::Kind::BgmColumn) {
            background = QColor(0x20, 0x24, 0x20);
        } else {
            background = palette::kRowAlternate;
        }
        painter.fillRect(QRectF(column.left, kHeaderHeight, column.width, height()), background);
        painter.setPen(QColor(0x14, 0x15, 0x18));
        painter.drawLine(QPointF(column.left, kHeaderHeight), QPointF(column.left, height()));
    }

    const auto& timing = controller_.project().timing;
    const int finest = controller_.snapDivisions() > 0 ? controller_.snapDivisions() : 16;
    const int divisions = viewport_.gridDivisionsFor(timing.resolution, kMinimumGridSpacingPixels, finest);
    const auto from = std::max<core::Pulse>(0, static_cast<core::Pulse>(std::floor(pulseAtY(height()))));
    const auto to = static_cast<core::Pulse>(std::ceil(pulseAtY(kHeaderHeight)));
    const double right = cols.empty() ? width() : cols.back().left + cols.back().width;
    for (const auto& line : core::gridLinesBetween(timing, divisions, from, to)) {
        const double y = yOf(static_cast<double>(line.pulse));
        painter.setPen(line.kind == core::GridLineKind::Bar    ? palette::kBarLine
                       : line.kind == core::GridLineKind::Beat ? palette::kBeatLine
                                                               : palette::kSubdivisionLine);
        painter.drawLine(QPointF(kRulerWidth, y), QPointF(right, y));
        if (line.kind == core::GridLineKind::Bar) {
            painter.setPen(palette::kText);
            painter.drawText(QRectF(0, y - 16, kRulerWidth - 4, 16), Qt::AlignRight | Qt::AlignBottom,
                             QString::number(line.barIndex));
        }
    }
}

void ChartEditorWidget::paintTiming(QPainter& painter) {
    const auto& timing = controller_.project().timing;
    painter.fillRect(QRectF(kRulerWidth, kHeaderHeight, kTimingWidth, height()), QColor(0x1a, 0x1b, 0x1f));
    const auto label = [&](core::Pulse pulse, const QString& text, const QColor& color, int row) {
        const double y = yOf(static_cast<double>(pulse));
        painter.setPen(color);
        painter.drawLine(QPointF(kRulerWidth, y), QPointF(kRulerWidth + kTimingWidth, y));
        painter.drawText(QRectF(kRulerWidth + 2, y - 14.0 * (row + 1), kTimingWidth - 4, 14), Qt::AlignLeft | Qt::AlignBottom, text);
    };
    label(0, QStringLiteral("%1").arg(timing.initialBpm, 0, 'g', 6), palette::kAutoCut, 0);
    for (const auto& event : timing.bpmEvents) {
        label(event.y, QStringLiteral("%1").arg(event.bpm, 0, 'g', 6), palette::kAutoCut, 0);
    }
    for (const auto& stop : timing.stopEvents) {
        label(stop.y, QStringLiteral("STOP %1").arg(stop.durationPulses), palette::kClippedCut, 1);
    }
    for (const auto& change : core::barLengthChangesOf(timing)) {
        const auto signature = core::timeSignatureForBarLength(change.lengthPulses, timing.resolution);
        const QString text = signature ? QStringLiteral("%1/%2").arg(signature->numerator).arg(signature->denominator)
                                       : QStringLiteral("%1p").arg(change.lengthPulses);
        label(core::barStartPulse(timing, change.bar), text, palette::kManualCut, 2);
    }
}

void ChartEditorWidget::paintCutMarks(QPainter& painter, const std::vector<Column>& cols) {
    const auto& project = controller_.project();
    const auto& selected = controller_.selectedCuts();
    const double top = kHeaderHeight;
    for (const auto& cut : project.cutPoints()) {
        const auto& placement = project.placement(cut.placementId);
        const auto column = std::find_if(cols.begin(), cols.end(), [&](const Column& candidate) {
            return candidate.kind == Column::Kind::Asset && candidate.asset == placement.assetId;
        });
        if (column == cols.end()) {
            continue;
        }
        const double y = yOf(static_cast<double>(project.absolutePulseOf(cut)));
        if (y < top - 4 || y > height() + 4) {
            continue;
        }
        QColor color = palette::assetColor(placement.assetId).lighter(130);
        const bool assigned = !project.notesOf(cut.id).empty();
        if (assigned) {
            color.setAlpha(110);
        }
        const QRectF mark(column->left + 3, y - 2, column->width - 6, 4);
        painter.fillRect(mark, color);
        if (cut.offsetPulses == 0) {
            painter.fillRect(QRectF(column->left + 1, y - 1, 2, 2), palette::kSelection);
        }
        if (project.tempoAffectedPlacements().contains(placement.id)) {
            painter.fillRect(QRectF(column->left + column->width - 3, y - 2, 2, 4), palette::kClippedCut);
        }
        if (std::find(selected.begin(), selected.end(), cut.id) != selected.end()) {
            painter.setPen(QPen(palette::kSelection, 1.5));
            painter.drawRect(mark.adjusted(-2, -2, 2, 2));
        }
    }
}

void ChartEditorWidget::paintNotes(QPainter& painter, const std::vector<Column>& cols) {
    const auto& project = controller_.project();
    const auto& selected = controller_.selectedNotes();
    std::map<std::pair<int, core::Pulse>, int> layers;
    for (const auto& note : project.notes()) {
        ++layers[{note.lane, project.absolutePulseOf(project.cutPoint(note.cutPointId))}];
    }
    for (const auto& note : project.notes()) {
        const auto column = std::find_if(cols.begin(), cols.end(), [&](const Column& candidate) {
            return candidate.kind != Column::Kind::Asset && candidate.x == note.lane;
        });
        if (column == cols.end()) {
            continue;
        }
        const auto& cut = project.cutPoint(note.cutPointId);
        const core::Pulse y = project.absolutePulseOf(cut);
        const bool dragging = drag_.moved && drag_.note == note.id &&
                              (drag_.kind == DragKind::NoteLane || drag_.kind == DragKind::NoteLength);
        Column target = *column;
        core::Pulse length = note.lengthPulses;
        if (dragging && drag_.kind == DragKind::NoteLane) {
            if (const auto hovered = columnAt(drag_.current.x()); hovered && hovered->kind != Column::Kind::Asset) {
                target = *hovered;
            }
        } else if (dragging) {
            length = std::max<core::Pulse>(0, drag_.pulse - y);
        }
        const double head = yOf(static_cast<double>(y));
        if (head < kHeaderHeight - 400 || yOf(static_cast<double>(y + length)) > height() + 10) {
            continue;
        }
        const QColor color = palette::assetColor(project.placement(cut.placementId).assetId).lighter(150);
        if (length > 0) {
            const double tail = yOf(static_cast<double>(y + length));
            QColor body = color;
            body.setAlpha(150);
            painter.fillRect(QRectF(target.left + target.width * 0.25, tail, target.width * 0.5, head - tail), body);
            painter.fillRect(QRectF(target.left + 1, tail, target.width - 2, 2), color);
        }
        const QRectF rect(target.left + 1, head - kNoteHeight, target.width - 2, kNoteHeight);
        painter.fillRect(rect, color);
        if (std::find(selected.begin(), selected.end(), note.id) != selected.end()) {
            painter.setPen(QPen(palette::kSelection, 1.5));
            painter.drawRect(rect.adjusted(-1, -1, 1, 1));
        }
        if (layers[{note.lane, y}] > 1) {
            painter.setPen(palette::kBackground);
            painter.drawText(rect, Qt::AlignCenter, QStringLiteral("%1").arg(layers[{note.lane, y}]));
        }
    }
}

void ChartEditorWidget::paintDensity(QPainter& painter) {
    const double left = width() - kDensityWidth;
    painter.fillRect(QRectF(left, kHeaderHeight, kDensityWidth, height()), QColor(0x18, 0x19, 0x1c));
    const auto* mode = controller_.keyMode();
    if (mode == nullptr) {
        return;
    }
    const auto& project = controller_.project();
    const auto counts = core::playableNotesPerBar(project, *mode);
    const std::size_t peak = counts.empty() ? 0 : *std::max_element(counts.begin(), counts.end());
    if (peak == 0) {
        return;
    }
    for (std::size_t bar = 0; bar < counts.size(); ++bar) {
        const double bottom = yOf(static_cast<double>(core::barStartPulse(project.timing, static_cast<int>(bar))));
        const double top = yOf(static_cast<double>(core::barStartPulse(project.timing, static_cast<int>(bar) + 1)));
        const double barWidth = (kDensityWidth - 4) * static_cast<double>(counts[bar]) / static_cast<double>(peak);
        painter.fillRect(QRectF(left + 2, top + 1, barWidth, std::max(1.0, bottom - top - 2)), QColor(0x8f, 0xc1, 0x8f, 160));
    }
}

void ChartEditorWidget::paintHeader(QPainter& painter, const std::vector<Column>& cols) {
    painter.fillRect(QRectF(0, 0, width(), kHeaderHeight), palette::kRuler);
    painter.setPen(palette::kDimText);
    painter.drawText(QRectF(kRulerWidth, 0, kTimingWidth, kHeaderHeight), Qt::AlignCenter, QStringLiteral("BPM 等"));
    const auto* mode = controller_.keyMode();
    for (const auto& column : cols) {
        QString label;
        if (column.kind == Column::Kind::Lane) {
            label = QString::number(column.x);
            if (mode != nullptr) {
                for (const auto& lane : mode->lanes()) {
                    if (lane.x == column.x) {
                        label = toQString(lane.label);
                    }
                }
            }
        } else if (column.kind == Column::Kind::BgmColumn) {
            label = QString::number(column.x);
        } else {
            label = toQString(controller_.project().asset(column.asset).name).left(2);
            painter.fillRect(QRectF(column.left, kHeaderHeight - 3, column.width, 3), palette::assetColor(column.asset));
        }
        painter.setPen(palette::kText);
        painter.drawText(QRectF(column.left, 0, column.width, kHeaderHeight - 3), Qt::AlignCenter, label);
    }
    painter.setPen(palette::kDimText);
    painter.drawText(QRectF(width() - kDensityWidth, 0, kDensityWidth, kHeaderHeight), Qt::AlignCenter, QStringLiteral("密度"));
    painter.setPen(mode_ == ChartEditMode::Assign ? palette::kAutoCut : palette::kManualCut);
    painter.drawText(QRectF(0, 0, kRulerWidth, kHeaderHeight), Qt::AlignCenter,
                     mode_ == ChartEditMode::Assign ? QStringLiteral("割当") : QStringLiteral("切断"));
}

void ChartEditorWidget::reportError(const std::exception& error) {
    emit controller_.statusMessage(toQString(error.what()));
}

void ChartEditorWidget::auditionCut(core::CutPointId cut) {
    controller_.setCursorPulse(controller_.project().absolutePulseOf(controller_.project().cutPoint(cut)));
    controller_.playSlice(cut);
}

void ChartEditorWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    drag_ = Drag{};
    if (event->button() != Qt::LeftButton || event->position().y() < kHeaderHeight) {
        return;
    }
    const QPointF point = event->position();
    const bool additive = event->modifiers().testFlag(Qt::ControlModifier);
    drag_.origin = point;
    drag_.current = point;

    bool onHandle = false;
    if (const auto note = noteAt(point, &onHandle)) {
        const auto& project = controller_.project();
        const auto cut = project.note(*note).cutPointId;
        auto notes = additive ? controller_.selectedNotes() : std::vector<core::NoteId>{};
        if (std::find(notes.begin(), notes.end(), *note) == notes.end()) {
            notes.push_back(*note);
        }
        controller_.setSelectedNotes(notes);
        controller_.focusCuts({cut});
        drag_.note = *note;
        drag_.kind = onHandle || event->modifiers().testFlag(Qt::ShiftModifier) ? DragKind::NoteLength : DragKind::NoteLane;
        drag_.pulse = project.absolutePulseOf(project.cutPoint(cut)) + project.note(*note).lengthPulses;
        return;
    }
    if (const auto cut = cutMarkAt(point)) {
        auto cuts = additive ? controller_.selectedCuts() : std::vector<core::CutPointId>{};
        const auto found = std::find(cuts.begin(), cuts.end(), *cut);
        if (found == cuts.end()) {
            cuts.push_back(*cut);
        } else if (additive) {
            cuts.erase(found);
        }
        controller_.setSelectedNotes(controller_.project().notesOf(*cut));
        controller_.focusCuts(cuts);
        if (!additive) {
            auditionCut(*cut);
        }
        if (mode_ == ChartEditMode::CutPoints && controller_.project().cutPoint(*cut).offsetPulses > 0) {
            drag_.kind = DragKind::CutMove;
            drag_.cut = *cut;
            drag_.pulse = controller_.project().absolutePulseOf(controller_.project().cutPoint(*cut));
        }
        return;
    }
    drag_.kind = DragKind::RubberBand;
}

void ChartEditorWidget::mouseMoveEvent(QMouseEvent* event) {
    if (drag_.kind == DragKind::None) {
        return;
    }
    drag_.current = event->position();
    const QPointF delta = drag_.current - drag_.origin;
    drag_.moved = drag_.moved || std::abs(delta.x()) + std::abs(delta.y()) >= kDragThresholdPixels;
    if (drag_.kind == DragKind::NoteLength || drag_.kind == DragKind::CutMove) {
        drag_.pulse = controller_.snap(pulseAtY(drag_.current.y()), StudioController::snapModeFor(event->modifiers()));
    }
    update();
}

void ChartEditorWidget::mouseReleaseEvent(QMouseEvent* event) {
    const Drag finished = drag_;
    drag_ = Drag{};
    try {
        switch (finished.kind) {
            case DragKind::NoteLane:
                if (finished.moved) {
                    const auto column = columnAt(finished.current.x());
                    if (column && column->kind != Column::Kind::Asset) {
                        app::changeNoteLane(controller_.document(), finished.note, column->x);
                    }
                }
                break;
            case DragKind::NoteLength:
                if (finished.moved) {
                    const auto& project = controller_.project();
                    const auto y = project.absolutePulseOf(project.cutPoint(project.note(finished.note).cutPointId));
                    app::changeNoteLength(controller_.document(), finished.note, std::max<core::Pulse>(0, finished.pulse - y));
                }
                break;
            case DragKind::CutMove:
                if (finished.moved) {
                    app::moveCut(controller_.document(), finished.cut, finished.pulse);
                }
                break;
            case DragKind::RubberBand:
                if (finished.moved) {
                    selectInRubberBand(QRectF(finished.origin, finished.current).normalized(),
                                       event->modifiers().testFlag(Qt::ControlModifier));
                } else {
                    controller_.setCursorPulse(
                        controller_.snap(pulseAtY(finished.origin.y()), StudioController::snapModeFor(event->modifiers())));
                    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
                        controller_.setSelectedNotes({});
                        controller_.setSelectedCuts({});
                    }
                }
                break;
            case DragKind::None: break;
        }
    } catch (const std::exception& error) {
        reportError(error);
    }
    update();
}

void ChartEditorWidget::selectInRubberBand(const QRectF& area, bool additive) {
    const auto& project = controller_.project();
    auto notes = additive ? controller_.selectedNotes() : std::vector<core::NoteId>{};
    auto cuts = additive ? controller_.selectedCuts() : std::vector<core::CutPointId>{};
    const auto cols = columns();
    for (const auto& note : project.notes()) {
        const auto column = std::find_if(cols.begin(), cols.end(), [&](const Column& candidate) {
            return candidate.kind != Column::Kind::Asset && candidate.x == note.lane;
        });
        if (column == cols.end()) {
            continue;
        }
        const QPointF center(column->left + column->width / 2.0,
                             yOf(static_cast<double>(project.absolutePulseOf(project.cutPoint(note.cutPointId)))) - kNoteHeight / 2.0);
        if (area.contains(center) && std::find(notes.begin(), notes.end(), note.id) == notes.end()) {
            notes.push_back(note.id);
        }
    }
    for (const auto& cut : project.cutPoints()) {
        const auto column = std::find_if(cols.begin(), cols.end(), [&](const Column& candidate) {
            return candidate.kind == Column::Kind::Asset && candidate.asset == project.placement(cut.placementId).assetId;
        });
        if (column == cols.end()) {
            continue;
        }
        const QPointF center(column->left + column->width / 2.0, yOf(static_cast<double>(project.absolutePulseOf(cut))));
        if (area.contains(center) && std::find(cuts.begin(), cuts.end(), cut.id) == cuts.end()) {
            cuts.push_back(cut.id);
        }
    }
    controller_.setSelectedNotes(notes);
    controller_.focusCuts(cuts);
}

void ChartEditorWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (mode_ != ChartEditMode::CutPoints || event->button() != Qt::LeftButton) {
        return;
    }
    const auto column = columnAt(event->position().x());
    if (!column || column->kind != Column::Kind::Asset || cutMarkAt(event->position())) {
        return;
    }
    const auto& project = controller_.project();
    const auto pulse = controller_.snap(pulseAtY(event->position().y()), StudioController::snapModeFor(event->modifiers()));
    std::optional<core::PlacementId> target;
    // Why not 一番近い配置を選ぶ: 切断点は配置の中にしか置けないので、その位置より前で始まる最後の配置（鳴っているはずの配置）に置く
    for (const auto& placement : project.placements()) {
        if (placement.assetId == column->asset && placement.startPulse < pulse &&
            (!target || project.placement(*target).startPulse < placement.startPulse)) {
            target = placement.id;
        }
    }
    if (!target) {
        emit controller_.statusMessage(QStringLiteral("この位置より前に、この素材の配置がありません"));
        return;
    }
    try {
        const auto cut = app::addManualCut(controller_.document(), *target, pulse);
        controller_.focusCuts({cut});
    } catch (const std::exception& error) {
        reportError(error);
    }
}

void ChartEditorWidget::wheelEvent(QWheelEvent* event) {
    const QPoint angle = event->angleDelta();
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        viewport_.zoomAround(height() - event->position().y(), std::pow(kWheelZoomStep, angle.y() / 120.0));
    } else if (event->modifiers().testFlag(Qt::ShiftModifier) || angle.x() != 0) {
        const int delta = angle.x() != 0 ? angle.x() : angle.y();
        horizontalOffset_ = std::max(0.0, horizontalOffset_ - delta / 2.0);
    } else {
        viewport_.scrollByPixels(angle.y() / 2.0);
    }
    update();
    event->accept();
}

void ChartEditorWidget::assignSelected(int lane) {
    auto cuts = controller_.selectedCuts();
    if (cuts.empty()) {
        for (const auto note : controller_.selectedNotes()) {
            cuts.push_back(controller_.project().note(note).cutPointId);
        }
    }
    if (cuts.empty()) {
        return;
    }
    try {
        const auto notes = app::assignLanes(controller_.document(), cuts, lane);
        controller_.setSelectedNotes(notes);
        if (autoAdvance_ && cuts.size() == 1) {
            stepCut(1);
        }
    } catch (const std::exception& error) {
        reportError(error);
    }
}

void ChartEditorWidget::clearSelected() {
    auto cuts = controller_.selectedCuts();
    for (const auto note : controller_.selectedNotes()) {
        cuts.push_back(controller_.project().note(note).cutPointId);
    }
    try {
        app::clearAssignments(controller_.document(), cuts);
        if (autoAdvance_ && controller_.selectedCuts().size() == 1) {
            stepCut(1);
        }
    } catch (const std::exception& error) {
        reportError(error);
    }
}

// Why not グリッド単位でカーソルを動かす: 割当モードではスライスの頭（切断点）にしかノーツを置けないので、
// 次の切断点へ直接飛ぶ方が、タイピングのように続けて割り当てられる
void ChartEditorWidget::stepCut(int direction) {
    const auto& project = controller_.project();
    auto ordered = core::cutPointsInSongOrder(project);
    std::optional<core::CutPointId> current;
    if (!controller_.selectedCuts().empty()) {
        current = controller_.selectedCuts().front();
        const auto asset = project.placement(project.cutPoint(*current).placementId).assetId;
        std::erase_if(ordered, [&](core::CutPointId id) { return project.placement(project.cutPoint(id).placementId).assetId != asset; });
    }
    if (ordered.empty()) {
        return;
    }
    std::ptrdiff_t index = 0;
    if (current) {
        const auto found = std::find(ordered.begin(), ordered.end(), *current);
        index = std::distance(ordered.begin(), found) + direction;
    } else {
        const auto cursor = controller_.cursorPulse();
        index = std::distance(ordered.begin(), std::find_if(ordered.begin(), ordered.end(), [&](core::CutPointId id) {
                                  return project.absolutePulseOf(project.cutPoint(id)) >= cursor;
                              }));
    }
    index = std::clamp<std::ptrdiff_t>(index, 0, static_cast<std::ptrdiff_t>(ordered.size()) - 1);
    const auto next = ordered[static_cast<std::size_t>(index)];
    controller_.setSelectedNotes(project.notesOf(next));
    controller_.focusCuts({next});
    scrollToPulse(static_cast<double>(project.absolutePulseOf(project.cutPoint(next))));
    auditionCut(next);
}

void ChartEditorWidget::keyPressEvent(QKeyEvent* event) {
    const int key = event->key();
    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    try {
        if (control && key == Qt::Key_C) {
            controller_.copySelectedNotes();
            return;
        }
        if (control && key == Qt::Key_V) {
            controller_.pasteAtCursor();
            return;
        }
        if (key >= Qt::Key_1 && key <= Qt::Key_9 && !control) {
            const auto* mode = controller_.keyMode();
            const auto keys = mode != nullptr ? core::keyLanesOf(*mode) : std::vector<int>{1, 2, 3, 4, 5, 6, 7};
            const auto index = static_cast<std::size_t>(key - Qt::Key_1);
            if (index < keys.size()) {
                assignSelected(keys[index]);
            }
            return;
        }
        switch (key) {
            case Qt::Key_S:
                if (scratchLane() > 0) {
                    assignSelected(scratchLane());
                }
                return;
            case Qt::Key_0:
            case Qt::Key_Backspace: clearSelected(); return;
            case Qt::Key_Delete:
                if (mode_ == ChartEditMode::CutPoints && !controller_.selectedCuts().empty()) {
                    app::removeCuts(controller_.document(), controller_.selectedCuts());
                } else {
                    app::removeNotes(controller_.document(), controller_.selectedNotes());
                }
                return;
            case Qt::Key_Up: stepCut(1); return;
            case Qt::Key_Down: stepCut(-1); return;
            case Qt::Key_Tab:
                setMode(mode_ == ChartEditMode::Assign ? ChartEditMode::CutPoints : ChartEditMode::Assign);
                return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (!controller_.selectedCuts().empty()) {
                    auditionCut(controller_.selectedCuts().front());
                }
                return;
            case Qt::Key_Space:
                if (controller_.isPlaying()) {
                    controller_.stopPlayback();
                } else {
                    controller_.playSongFromCursor();
                }
                return;
            default: break;
        }
    } catch (const std::exception& error) {
        reportError(error);
        return;
    }
    QWidget::keyPressEvent(event);
}

// Why not Tab でフォーカスを移す: 譜面ビューでは Tab をモード切替に使うため、フォーカスの移動を止めてキー入力として受け取る
bool ChartEditorWidget::focusNextPrevChild(bool) { return false; }

void ChartEditorWidget::contextMenuEvent(QContextMenuEvent* event) {
    const auto pulse = controller_.snap(pulseAtY(event->pos().y()), app::SnapMode::Grid);
    showTimingMenu(event->globalPos(), pulse);
}

void ChartEditorWidget::showTimingMenu(QPoint globalPosition, core::Pulse pulse) {
    const auto& timing = controller_.project().timing;
    const int bar = core::barIndexAt(timing, pulse);
    const auto edit = [this](const QString& label, std::function<void(core::TimingData&)> change) {
        // Why not 例外をそのまま投げる: メニューの処理中に投げた例外は Qt のイベントループを通り抜けられず、アプリが落ちるため
        try {
            controller_.editTiming(label, change);
        } catch (const std::exception& error) {
            reportError(error);
        }
    };
    QMenu menu(this);
    menu.addSection(QStringLiteral("小節 %1 / pulse %2").arg(bar).arg(pulse));
    menu.addAction(QStringLiteral("ここで BPM を変える…"), this, [this, pulse, edit] {
        bool ok = false;
        const double current = controller_.project().timing.toTimingMap().bpmAt(pulse);
        const double bpm = QInputDialog::getDouble(this, QStringLiteral("BPM"), QStringLiteral("pulse %1 からの BPM").arg(pulse),
                                                   current, 1.0, 9999.0, 3, &ok);
        if (ok) {
            edit(QStringLiteral("BPM の変更"), [&](core::TimingData& edited) { core::setBpmChange(edited, pulse, bpm); });
        }
    });
    if (std::any_of(timing.bpmEvents.begin(), timing.bpmEvents.end(), [pulse](const auto& event) { return event.y == pulse; })) {
        menu.addAction(QStringLiteral("この BPM 変更を消す"), this, [this, pulse, edit] {
            edit(QStringLiteral("BPM 変更の削除"), [&](core::TimingData& edited) { core::removeBpmChange(edited, pulse); });
        });
    }
    menu.addAction(QStringLiteral("ここに STOP を置く…"), this, [this, pulse, edit] {
        bool ok = false;
        const int length = QInputDialog::getInt(this, QStringLiteral("STOP"), QStringLiteral("止める長さ（pulse。4 分音符 = %1）")
                                                                                 .arg(controller_.project().timing.resolution),
                                                controller_.project().timing.resolution, 1, 1000000, 1, &ok);
        if (ok) {
            edit(QStringLiteral("STOP の追加"), [&](core::TimingData& edited) { core::setStop(edited, pulse, length); });
        }
    });
    if (std::any_of(timing.stopEvents.begin(), timing.stopEvents.end(), [pulse](const auto& event) { return event.y == pulse; })) {
        menu.addAction(QStringLiteral("この STOP を消す"), this, [this, pulse, edit] {
            edit(QStringLiteral("STOP の削除"), [&](core::TimingData& edited) { core::removeStop(edited, pulse); });
        });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("小節 %1 から拍子を変える…").arg(bar), this, [this, bar, edit] {
        bool ok = false;
        const QString text = QInputDialog::getText(this, QStringLiteral("拍子"), QStringLiteral("拍子（例: 7/8）"),
                                                   QLineEdit::Normal, QStringLiteral("4/4"), &ok);
        const auto match = QRegularExpression(QStringLiteral("^\\s*(\\d+)\\s*/\\s*(\\d+)\\s*$")).match(text);
        if (!ok) {
            return;
        }
        if (!match.hasMatch()) {
            emit controller_.statusMessage(QStringLiteral("拍子は 7/8 のように入力してください"));
            return;
        }
        const core::TimeSignature signature{match.captured(1).toInt(), match.captured(2).toInt()};
        const auto end = app::songEndPulse(controller_.project());
        edit(QStringLiteral("拍子の変更"), [&](core::TimingData& edited) { core::setTimeSignature(edited, bar, signature, end); });
    });
    const auto changes = core::barLengthChangesOf(timing);
    if (bar > 0 && std::any_of(changes.begin(), changes.end(), [bar](const auto& change) { return change.bar == bar; })) {
        menu.addAction(QStringLiteral("小節 %1 の拍子変更を消す").arg(bar), this, [this, bar, edit] {
            const auto end = app::songEndPulse(controller_.project());
            edit(QStringLiteral("拍子変更の削除"),
                                   [&](core::TimingData& edited) { core::removeTimeSignatureChange(edited, bar, end); });
        });
    }
    const auto starts = core::barStartsOf(timing);
    if (std::find(starts.begin(), starts.end(), pulse) == starts.end()) {
        menu.addAction(QStringLiteral("ここに小節線を足す"), this, [this, pulse, edit] {
            const auto end = app::songEndPulse(controller_.project());
            edit(QStringLiteral("小節線の追加"), [&](core::TimingData& edited) { core::insertBarLine(edited, pulse, end); });
        });
    } else if (pulse > 0 && timing.barLines.has_value()) {
        menu.addAction(QStringLiteral("この小節線を消す"), this, [this, pulse, edit] {
            edit(QStringLiteral("小節線の削除"), [&](core::TimingData& edited) { core::removeBarLine(edited, pulse); });
        });
    }
    menu.exec(globalPosition);
}

}  // namespace ibs::gui
