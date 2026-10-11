#include "ibs/app/TimelineViewport.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "ibs/core/slicing/CutPlanner.hpp"

namespace ibs::app {

namespace {

constexpr int kFineSnapMultiplier = 4;

}  // namespace

TimelineViewport::TimelineViewport(double pulsesPerPixel, double minimumPulsesPerPixel, double maximumPulsesPerPixel)
    : pulsesPerPixel_(pulsesPerPixel), minimumPulsesPerPixel_(minimumPulsesPerPixel),
      maximumPulsesPerPixel_(maximumPulsesPerPixel) {
    if (minimumPulsesPerPixel <= 0.0 || maximumPulsesPerPixel < minimumPulsesPerPixel) {
        throw std::invalid_argument("invalid zoom limits");
    }
    pulsesPerPixel_ = std::clamp(pulsesPerPixel, minimumPulsesPerPixel_, maximumPulsesPerPixel_);
}

double TimelineViewport::xOf(double pulse) const noexcept { return (pulse - originPulse_) / pulsesPerPixel_; }

double TimelineViewport::pulseAt(double x) const noexcept { return originPulse_ + x * pulsesPerPixel_; }

void TimelineViewport::setOrigin(double pulse) { originPulse_ = std::max(0.0, pulse); }

void TimelineViewport::scrollByPixels(double pixels) { setOrigin(originPulse_ + pixels * pulsesPerPixel_); }

void TimelineViewport::setPulsesPerPixel(double pulsesPerPixel) {
    pulsesPerPixel_ = std::clamp(pulsesPerPixel, minimumPulsesPerPixel_, maximumPulsesPerPixel_);
}

void TimelineViewport::zoomAround(double x, double factor) {
    if (factor <= 0.0) {
        return;
    }
    const double anchor = pulseAt(x);
    setPulsesPerPixel(pulsesPerPixel_ / factor);
    setOrigin(anchor - x * pulsesPerPixel_);
}

void TimelineViewport::fit(double fromPulse, double toPulse, double widthPixels) {
    if (widthPixels <= 0.0 || toPulse <= fromPulse) {
        return;
    }
    setPulsesPerPixel((toPulse - fromPulse) / widthPixels);
    setOrigin(fromPulse);
}

// Why not 再生位置を常に画面の同じ所に保つ: 毎フレーム全体が流れると波形やノーツを目で追えないため、
// 端に近づいたときだけ 1 画面分めくる
bool TimelineViewport::follow(double pulse, double widthPixels) {
    constexpr double kTurnAt = 0.95;
    constexpr double kLead = 0.05;
    const double span = widthPixels * pulsesPerPixel_;
    if (widthPixels <= 0.0 || (pulse >= originPulse_ && pulse <= originPulse_ + span * kTurnAt)) {
        return false;
    }
    setOrigin(pulse - span * kLead);
    return true;
}

int TimelineViewport::gridDivisionsFor(int resolution, double minimumSpacingPixels, int finestDivisions) const {
    // Why not 指定されたグリッドを常に描く: 縮小表示では線が画面を埋め尽くして波形が見えなくなるため、
    // 線の間隔が最小間隔を下回る間は分割を半分にしていく
    int divisions = std::max(1, finestDivisions);
    while (divisions > 1 && (static_cast<double>(resolution) * 4.0 / divisions) / pulsesPerPixel_ < minimumSpacingPixels) {
        if (divisions % 2 == 0) {
            divisions /= 2;
        } else if (divisions % 3 == 0) {
            divisions /= 3;
        } else {
            divisions = 1;
        }
    }
    return divisions;
}

core::Pulse snapPulse(const core::TimingData& timing, double pulse, int divisionsPerWholeNote, SnapMode mode) {
    const double clamped = std::max(0.0, pulse);
    if (mode == SnapMode::Off || divisionsPerWholeNote <= 0) {
        return static_cast<core::Pulse>(std::llround(clamped));
    }
    const int divisions = mode == SnapMode::Fine ? divisionsPerWholeNote * kFineSnapMultiplier : divisionsPerWholeNote;
    return static_cast<core::Pulse>(std::llround(core::BeatGrid(timing, divisions).nearestGridPulse(clamped)));
}

}  // namespace ibs::app
