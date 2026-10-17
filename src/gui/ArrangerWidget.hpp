#pragma once

#include <algorithm>
#include <functional>
#include <optional>

#include <QWidget>

#include "StudioController.hpp"
#include "ibs/app/TimelineViewport.hpp"

namespace ibs::gui {

class ArrangerWidget final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kHeaderWidth = 150;
    static constexpr int kRulerHeight = 22;
    static constexpr int kRowHeight = 46;

    explicit ArrangerWidget(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] app::TimelineViewport& viewport() noexcept { return viewport_; }
    [[nodiscard]] QRectF placementRect(core::PlacementId placement) const;
    [[nodiscard]] int rowTop(std::size_t row) const;

    void zoomToFit();
    [[nodiscard]] double contentEndPulse() const;
    [[nodiscard]] double timelineWidthPixels() const { return std::max(1, width() - kHeaderWidth); }
    void setViewObserver(std::function<void()> observer) { viewObserver_ = std::move(observer); }

signals:
    void placementActivated(core::PlacementId placement);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Drag {
        core::PlacementId placement;
        double grabOffsetPulses = 0.0;
        core::Pulse originalStart = 0;
        core::Pulse currentStart = 0;
        bool duplicate = false;
        bool moved = false;
    };

    void showAssetMenu(core::AssetId asset, QPoint globalPosition);
    void removeAssetWithConfirmation(core::AssetId asset);
    void copyCutLayoutFrom(core::AssetId source);
    [[nodiscard]] std::optional<std::size_t> rowAt(double y) const;
    [[nodiscard]] std::optional<core::PlacementId> placementAt(QPointF point) const;
    [[nodiscard]] core::Pulse placementEnd(const core::Placement& placement) const;
    [[nodiscard]] double timelineX(double pulse) const;
    [[nodiscard]] double pulseAtX(double x) const;
    void paintRuler(QPainter& painter);
    void paintRows(QPainter& painter);
    void paintPlacement(QPainter& painter, const core::Placement& placement, core::Pulse start, bool ghost);
    void reportError(const std::exception& error);

    StudioController& controller_;
    app::TimelineViewport viewport_{8.0, 0.25, 2048.0};
    double verticalOffset_ = 0.0;
    std::optional<Drag> drag_;
    std::function<void()> viewObserver_;
};

}  // namespace ibs::gui
