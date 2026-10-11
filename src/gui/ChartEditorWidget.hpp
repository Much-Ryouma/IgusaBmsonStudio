#pragma once

#include <optional>
#include <vector>

#include <QWidget>

#include "StudioController.hpp"
#include "ibs/app/TimelineViewport.hpp"

namespace ibs::gui {

enum class ChartEditMode { Assign, CutPoints };

class ChartEditorWidget final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kHeaderHeight = 22;
    static constexpr int kRulerWidth = 34;
    static constexpr int kTimingWidth = 64;
    static constexpr int kLaneUnitWidth = 26;
    static constexpr int kBgmColumnWidth = 22;
    static constexpr int kAssetColumnWidth = 20;
    static constexpr int kDensityWidth = 28;
    static constexpr int kNoteHeight = 6;
    static constexpr int kBgaColumnWidth = 34;

    struct Column {
        enum class Kind { Lane, BgmColumn, Bga, Asset } kind = Kind::Lane;
        int x = 0;
        core::AssetId asset;
        double left = 0.0;
        double width = 0.0;
    };

    explicit ChartEditorWidget(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] app::TimelineViewport& viewport() noexcept { return viewport_; }
    [[nodiscard]] ChartEditMode mode() const noexcept { return mode_; }
    void setMode(ChartEditMode mode);
    [[nodiscard]] bool autoAdvance() const noexcept { return autoAdvance_; }
    void setAutoAdvance(bool enabled) { autoAdvance_ = enabled; }
    void setBgmColumnsCollapsed(bool collapsed);

    [[nodiscard]] std::vector<Column> columns() const;
    [[nodiscard]] std::optional<Column> laneColumn(int x) const;
    [[nodiscard]] std::optional<Column> assetColumn(core::AssetId asset) const;
    [[nodiscard]] std::optional<Column> bgaColumn(core::BgaTrack track) const;
    [[nodiscard]] double yOf(double pulse) const;
    [[nodiscard]] double pulseAtY(double y) const;
    void scrollToPulse(double pulse);

    void assignSelected(int lane);
    void clearSelected();
    void stepCut(int direction);

signals:
    void modeChanged(ChartEditMode mode);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    enum class DragKind { None, NoteLane, NoteLength, CutMove, BgaMove, RubberBand };
    struct Drag {
        DragKind kind = DragKind::None;
        QPointF origin;
        QPointF current;
        core::NoteId note;
        core::CutPointId cut;
        BgaEventRef bga;
        core::Pulse pulse = 0;
        bool moved = false;
    };

    [[nodiscard]] std::optional<Column> columnAt(double x) const;
    [[nodiscard]] std::optional<core::NoteId> noteAt(QPointF point, bool* onLengthHandle = nullptr) const;
    [[nodiscard]] std::optional<core::CutPointId> cutMarkAt(QPointF point) const;
    [[nodiscard]] double timelineHeight() const;
    [[nodiscard]] int scratchLane() const;
    [[nodiscard]] std::vector<int> bgmColumnLanes() const;
    void paintGrid(QPainter& painter, const std::vector<Column>& columns);
    void paintTiming(QPainter& painter);
    void paintCutMarks(QPainter& painter, const std::vector<Column>& columns);
    void paintNotes(QPainter& painter, const std::vector<Column>& columns);
    void paintBga(QPainter& painter, const std::vector<Column>& columns);
    [[nodiscard]] std::optional<BgaEventRef> bgaEventAt(QPointF point) const;
    void paintDensity(QPainter& painter);
    void paintHeader(QPainter& painter, const std::vector<Column>& columns);
    void selectInRubberBand(const QRectF& area, bool additive);
    void auditionCut(core::CutPointId cut);
    void reportError(const std::exception& error);
    void showTimingMenu(QPoint globalPosition, core::Pulse pulse);

    StudioController& controller_;
    app::TimelineViewport viewport_{4.0, 0.1, 64.0};
    ChartEditMode mode_ = ChartEditMode::Assign;
    bool autoAdvance_ = true;
    bool bgmCollapsed_ = false;
    double horizontalOffset_ = 0.0;
    Drag drag_;
};

}  // namespace ibs::gui
