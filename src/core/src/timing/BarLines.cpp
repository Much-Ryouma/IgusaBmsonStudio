#include "ibs/core/timing/BarLines.hpp"

#include <algorithm>
#include <array>

namespace ibs::core {

std::vector<Pulse> barStartsOf(const TimingData& timing) {
    std::vector<Pulse> starts{0};
    if (timing.barLines.has_value()) {
        for (const Pulse line : *timing.barLines) {
            if (line > 0) {
                starts.push_back(line);
            }
        }
    }
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    return starts;
}

std::optional<TimeSignature> timeSignatureForBarLength(Pulse barLengthPulses, int resolution) {
    constexpr std::array<int, 7> kDenominators = {1, 2, 4, 8, 16, 32, 64};
    constexpr int kMaximumNumerator = 255;
    const Pulse wholeNote = static_cast<Pulse>(resolution) * 4;
    if (barLengthPulses <= 0 || wholeNote <= 0) {
        return std::nullopt;
    }
    for (const int denominator : kDenominators) {
        const Pulse scaled = barLengthPulses * denominator;
        if (scaled % wholeNote == 0) {
            const Pulse numerator = scaled / wholeNote;
            if (numerator > kMaximumNumerator) {
                return std::nullopt;
            }
            // Why not 分母 1 や 2 をそのまま使う: 4/4 が 1/1 になるなど DAW で読みにくい拍子になるため、4 分音符単位にそろえる
            // (小節線の長さだけでは 6/8 と 3/4 を区別できない。拍子の意図は M4 の拍子トラックで持つ)
            if (denominator < 4 && (numerator * (4 / denominator)) <= kMaximumNumerator) {
                return TimeSignature{static_cast<int>(numerator * (4 / denominator)), 4};
            }
            return TimeSignature{static_cast<int>(numerator), denominator};
        }
    }
    return std::nullopt;
}

}  // namespace ibs::core
