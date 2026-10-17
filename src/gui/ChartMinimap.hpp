#pragma once

#include <QImage>
#include <QWidget>

#include "StudioController.hpp"

namespace ibs::gui {

class ChartEditorWidget;

class ChartMinimap final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kWidth = 64;

    ChartMinimap(StudioController& controller, ChartEditorWidget& editor, QWidget* parent = nullptr);

    [[nodiscard]] double yOfPulse(double pulse) const;
    [[nodiscard]] double pulseAtY(double y) const;
    [[nodiscard]] QRectF viewportRect() const;

    void refreshView();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void centerOn(double y);
    void rebuildContent();

    StudioController& controller_;
    ChartEditorWidget& editor_;
    QImage content_;
    bool contentDirty_ = true;
    double contentEnd_ = 1.0;
    double shownOrigin_ = -1.0;
    double shownSpan_ = -1.0;
};

}  // namespace ibs::gui
