#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QWidget>

#include "StudioController.hpp"
#include "ibs/app/TimelineViewport.hpp"

namespace ibs::gui {

class WaveformSlicerWidget final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kRulerHeight = 20;
    static constexpr double kGrabDistancePixels = 5.0;

    explicit WaveformSlicerWidget(StudioController& controller, QWidget* parent = nullptr);

    [[nodiscard]] app::TimelineViewport& viewport() noexcept { return viewport_; }
    [[nodiscard]] double xOfSongPulse(double pulse) const { return viewport_.xOf(pulse); }
    void fitPlacement();
    void revealCut(core::CutPointId cut, bool zoomToSlice);
    void revealRange(double fromPulse, double toPulse, bool zoomToRange);
    [[nodiscard]] double contentStartPulse() const;
    [[nodiscard]] double contentEndPulse() const;
    void setViewObserver(std::function<void()> observer) { viewObserver_ = std::move(observer); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct CutDrag {
        core::CutPointId cut;
        core::Pulse currentPulse = 0;
        bool moved = false;
    };
    struct RubberBand {
        double fromX = 0.0;
        double toX = 0.0;
    };

    [[nodiscard]] std::optional<core::CutPointId> cutNear(double x) const;
    [[nodiscard]] std::optional<core::CutPointId> sliceAt(double songPulse) const;
    [[nodiscard]] core::Pulse placementStart() const;
    [[nodiscard]] core::Pulse placementEnd() const;
    void onSelectionChanged();
    void stepSlice(int direction);
    void paintGrid(QPainter& painter);
    void paintWaveform(QPainter& painter);
    void paintCuts(QPainter& painter);
    void paintPreview(QPainter& painter);
    void reportError(const std::exception& error);

    StudioController& controller_;
    app::TimelineViewport viewport_{4.0, 0.02, 2048.0};
    std::optional<core::PlacementId> shownPlacement_;
    std::optional<CutDrag> drag_;
    std::optional<RubberBand> rubberBand_;
    bool needsFit_ = false;
    std::function<void()> viewObserver_;
};

}  // namespace ibs::gui
