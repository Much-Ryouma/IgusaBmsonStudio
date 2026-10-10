#include "ibs/core/audio/WaveformPyramid.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ibs::core {

namespace {

WaveformLevel baseLevel(const std::vector<float>& mono, std::size_t samplesPerBin) {
    WaveformLevel level{samplesPerBin, {}, {}};
    const std::size_t bins = (mono.size() + samplesPerBin - 1) / samplesPerBin;
    level.minimum.assign(bins, 0.0f);
    level.maximum.assign(bins, 0.0f);
    for (std::size_t bin = 0; bin < bins; ++bin) {
        const auto first = mono.begin() + static_cast<std::ptrdiff_t>(bin * samplesPerBin);
        const auto last = mono.begin() + static_cast<std::ptrdiff_t>(std::min(mono.size(), (bin + 1) * samplesPerBin));
        const auto [lowest, highest] = std::minmax_element(first, last);
        level.minimum[bin] = *lowest;
        level.maximum[bin] = *highest;
    }
    return level;
}

WaveformLevel reducedLevel(const WaveformLevel& finer, std::size_t factor) {
    WaveformLevel level{finer.samplesPerBin * factor, {}, {}};
    const std::size_t bins = (finer.minimum.size() + factor - 1) / factor;
    level.minimum.assign(bins, 0.0f);
    level.maximum.assign(bins, 0.0f);
    for (std::size_t bin = 0; bin < bins; ++bin) {
        const std::size_t begin = bin * factor;
        const std::size_t end = std::min(finer.minimum.size(), begin + factor);
        level.minimum[bin] = *std::min_element(finer.minimum.begin() + static_cast<std::ptrdiff_t>(begin),
                                               finer.minimum.begin() + static_cast<std::ptrdiff_t>(end));
        level.maximum[bin] = *std::max_element(finer.maximum.begin() + static_cast<std::ptrdiff_t>(begin),
                                               finer.maximum.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return level;
}

}  // namespace

WaveformPyramid::WaveformPyramid(const std::vector<float>& mono, std::size_t baseSamplesPerBin,
                                 std::size_t reductionFactor, std::size_t levelCount)
    : sampleCount_(mono.size()) {
    if (baseSamplesPerBin == 0 || reductionFactor < 2 || levelCount == 0) {
        throw std::invalid_argument("invalid waveform pyramid configuration");
    }
    if (mono.empty()) {
        return;
    }
    levels_.push_back(baseLevel(mono, baseSamplesPerBin));
    while (levels_.size() < levelCount && levels_.back().minimum.size() > 1) {
        levels_.push_back(reducedLevel(levels_.back(), reductionFactor));
    }
}

std::pair<float, float> WaveformPyramid::range(SampleIndex begin, SampleIndex end) const {
    const auto clampedBegin = static_cast<std::size_t>(std::clamp<SampleIndex>(begin, 0, static_cast<SampleIndex>(sampleCount_)));
    const auto clampedEnd = static_cast<std::size_t>(std::clamp<SampleIndex>(end, 0, static_cast<SampleIndex>(sampleCount_)));
    if (levels_.empty() || clampedEnd <= clampedBegin) {
        return {0.0f, 0.0f};
    }
    // Why not 常に最も細かい段を使う: 1 画素に数万サンプルが入る縮小表示では、毎回の描画で数万回の比較が必要になり重くなるため、
    // 1 画素の幅を超えない範囲で最も粗い段を選ぶ
    const std::size_t span = clampedEnd - clampedBegin;
    const WaveformLevel* level = &levels_.front();
    for (const auto& candidate : levels_) {
        if (candidate.samplesPerBin <= span) {
            level = &candidate;
        }
    }
    const std::size_t firstBin = clampedBegin / level->samplesPerBin;
    const std::size_t lastBin = std::min(level->minimum.size(), (clampedEnd + level->samplesPerBin - 1) / level->samplesPerBin);
    float lowest = std::numeric_limits<float>::max();
    float highest = std::numeric_limits<float>::lowest();
    for (std::size_t bin = firstBin; bin < lastBin; ++bin) {
        lowest = std::min(lowest, level->minimum[bin]);
        highest = std::max(highest, level->maximum[bin]);
    }
    return {lowest, highest};
}

}  // namespace ibs::core
