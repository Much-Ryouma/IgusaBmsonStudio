#include "ChartMinimap.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

#include "ChartEditorWidget.hpp"
#include "StudioPalette.hpp"

namespace ibs::gui {

namespace {

constexpr double kMargin = 4.0;
constexpr double kLaneShare = 0.7;

}  // namespace

ChartMinimap::ChartMinimap(StudioController& controller, ChartEditorWidget& editor, QWidget* parent)
    : QWidget(parent), controller_(controller), editor_(editor) {
    setFixedWidth(kWidth);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("譜面全体の縮図。クリック・ドラッグでその位置へ移動します"));
    connect(&controller_, &StudioController::documentChanged, this, [this] {
        contentDirty_ = true;
        update();
    });
    const auto refresh = [this] { update(); };
    connect(&controller_, &StudioController::cursorChanged, this, refresh);
    connect(&controller_, &StudioController::playheadChanged, this, [this] {
        if (controller_.songPlayheadPulse() || controller_.isPlaying()) {
            update();
        }
    });
}

double ChartMinimap::yOfPulse(double pulse) const {
    const double usable = std::max(1.0, height() - 2.0 * kMargin);
    return height() - kMargin - usable * pulse / contentEnd_;
}

double ChartMinimap::pulseAtY(double y) const {
    const double usable = std::max(1.0, height() - 2.0 * kMargin);
    return std::max(0.0, (height() - kMargin - y) / usable * contentEnd_);
}

QRectF ChartMinimap::viewportRect() const {
    const double origin = editor_.viewport().originPulse();
    const double top = yOfPulse(origin + editor_.visibleSpanPulses());
    const double bottom = yOfPulse(origin);
    return QRectF(1.0, top, width() - 2.0, std::max(4.0, bottom - top));
}

// Why not 毎回すべてのノーツを描く: 譜面の再描画（スクロール・再生中の更新）のたびに数千のノーツを
// 位置計算し直すと、それだけで 1 フレームが数百 ms かかったため、譜面が変わったときだけ画像に描いておく
void ChartMinimap::refreshView() {
    const double origin = editor_.viewport().originPulse();
    const double span = editor_.visibleSpanPulses();
    if (contentDirty_ || origin != shownOrigin_ || span != shownSpan_) {
        update();
    }
}

void ChartMinimap::resizeEvent(QResizeEvent* event) {
    contentDirty_ = true;
    QWidget::resizeEvent(event);
}

// Why not 本物の譜面を縮小して描く: 曲が長いと 1 画素に何小節も入り、レーンの区別がつかないため、
// 鍵盤のノーツを左に、割り当てていない切断点（BGM）を右に分けて、どこが混んでいるかだけが分かるようにする
void ChartMinimap::rebuildContent() {
    contentDirty_ = false;
    contentEnd_ = std::max(1.0, editor_.contentEndPulse());
    content_ = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    content_.fill(QColor(0x18, 0x19, 0x1c));
    QPainter painter(&content_);
    const auto& project = controller_.project();
    const auto* mode = controller_.keyMode();
    std::map<int, int> laneIndex;
    if (mode != nullptr) {
        for (const auto& lane : mode->lanes()) {
            laneIndex.emplace(lane.x, static_cast<int>(laneIndex.size()));
        }
    }
    const int laneCount = std::max<int>(1, static_cast<int>(laneIndex.size()));
    const double laneArea = (width() - 4.0) * kLaneShare;
    const double laneWidth = laneArea / laneCount;
    const double bgmLeft = 2.0 + laneArea + 2.0;

    std::map<std::uint64_t, core::Pulse> placementStarts;
    for (const auto& placement : project.placements()) {
        placementStarts.emplace(placement.id.value, placement.startPulse);
    }
    std::map<std::uint64_t, std::pair<core::Pulse, core::AssetId>> cutPositions;
    for (const auto& cut : project.cutPoints()) {
        cutPositions.emplace(cut.id.value,
                             std::pair{placementStarts[cut.placementId.value] + cut.offsetPulses, project.placement(cut.placementId).assetId});
    }
    std::set<std::uint64_t> assigned;
    for (const auto& note : project.notes()) {
        assigned.insert(note.cutPointId.value);
        const auto& [pulse, asset] = cutPositions[note.cutPointId.value];
        const double y = yOfPulse(static_cast<double>(pulse));
        QColor color = palette::assetColor(asset).lighter(150);
        const auto found = laneIndex.find(note.lane);
        if (found == laneIndex.end()) {
            color.setAlpha(120);
            painter.fillRect(QRectF(bgmLeft, y - 1.0, width() - bgmLeft - 2.0, 1.5), color);
            continue;
        }
        painter.fillRect(QRectF(2.0 + found->second * laneWidth, y - 1.0, std::max(1.0, laneWidth - 1.0), 2.0), color);
    }
    QColor bgm = palette::kDimText;
    bgm.setAlpha(90);
    for (const auto& [id, position] : cutPositions) {
        if (!assigned.contains(id)) {
            painter.fillRect(QRectF(bgmLeft, yOfPulse(static_cast<double>(position.first)) - 0.5, width() - bgmLeft - 2.0, 1.0), bgm);
        }
    }
}

void ChartMinimap::paintEvent(QPaintEvent*) {
    if (contentDirty_ || content_.size() != size()) {
        rebuildContent();
    }
    QPainter painter(this);
    painter.drawImage(0, 0, content_);
    painter.setPen(QPen(palette::kCursor, 1.0));
    const double cursor = yOfPulse(static_cast<double>(controller_.cursorPulse()));
    painter.drawLine(QPointF(0, cursor), QPointF(width(), cursor));
    if (const auto playhead = controller_.songPlayheadPulse()) {
        painter.setPen(QPen(palette::kPlayhead, 1.5));
        painter.drawLine(QPointF(0, yOfPulse(*playhead)), QPointF(width(), yOfPulse(*playhead)));
    }
    shownOrigin_ = editor_.viewport().originPulse();
    shownSpan_ = editor_.visibleSpanPulses();
    const QRectF view = viewportRect();
    painter.fillRect(view, QColor(255, 255, 255, 36));
    painter.setPen(QPen(QColor(255, 255, 255, 140), 1.0));
    painter.drawRect(view.adjusted(0.5, 0.5, -0.5, -0.5));
}

void ChartMinimap::centerOn(double y) {
    if (contentDirty_ || content_.size() != size()) {
        rebuildContent();
    }
    editor_.viewport().setOrigin(pulseAtY(y) - editor_.visibleSpanPulses() / 2.0);
    editor_.update();
    update();
}

void ChartMinimap::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        centerOn(event->position().y());
    }
}

void ChartMinimap::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons().testFlag(Qt::LeftButton)) {
        centerOn(event->position().y());
    }
}

void ChartMinimap::wheelEvent(QWheelEvent* event) {
    QApplication::sendEvent(&editor_, event);
    update();
}

}  // namespace ibs::gui
