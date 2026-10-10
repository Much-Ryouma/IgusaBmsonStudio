#pragma once

#include <cmath>

#include "ibs/core/timing/TimingMap.hpp"

namespace ibs::gui {

// Why not 整数の pulse で秒を求める: 拡大表示では 1 pulse が数画素になり、波形が階段状に描かれるため、pulse の間を補間する
inline double secondsAtFractionalPulse(const core::TimingMap& timing, double pulse) {
    const double floor = std::floor(pulse);
    const auto whole = static_cast<core::Pulse>(floor);
    const double base = timing.secondsAt(whole);
    return base + (timing.secondsAt(whole + 1) - base) * (pulse - floor);
}

}  // namespace ibs::gui
