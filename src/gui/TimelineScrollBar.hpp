#pragma once

#include <algorithm>
#include <functional>

#include <QScrollBar>
#include <QSignalBlocker>
#include <QWidget>

#include "ibs/app/TimelineViewport.hpp"

namespace ibs::gui {

class TimelineScrollBar final : public QScrollBar {
    Q_OBJECT

public:
    struct Binding {
        app::TimelineViewport* viewport = nullptr;
        QWidget* target = nullptr;
        std::function<double()> contentStartPulse;
        std::function<double()> contentEndPulse;
        std::function<double()> visibleWidthPixels;
    };

    TimelineScrollBar(Binding binding, QWidget* parent = nullptr) : QScrollBar(Qt::Horizontal, parent), binding_(std::move(binding)) {
        connect(this, &QScrollBar::valueChanged, this, [this](int value) {
            binding_.viewport->setOrigin(static_cast<double>(value));
            binding_.target->update();
        });
    }

    // Why not 毎回スクロールバーから位置を決める: 拡大縮小・ホイール・自動スクロールでも表示位置が変わるため、
    // 表示の側を正としてスクロールバーを合わせる
    void sync() {
        const QSignalBlocker blocker(this);
        const double visible = binding_.visibleWidthPixels() * binding_.viewport->pulsesPerPixel();
        const double start = std::max(0.0, std::min(binding_.contentStartPulse(), binding_.viewport->originPulse()));
        const double end = std::max(binding_.contentEndPulse(), binding_.viewport->originPulse() + visible);
        setRange(static_cast<int>(start), static_cast<int>(std::max(start, end - visible)));
        setPageStep(std::max(1, static_cast<int>(visible)));
        setSingleStep(std::max(1, static_cast<int>(visible / 10.0)));
        setValue(static_cast<int>(binding_.viewport->originPulse()));
    }

private:
    Binding binding_;
};

}  // namespace ibs::gui
