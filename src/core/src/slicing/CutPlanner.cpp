#include "ibs/core/slicing/CutPlanner.hpp"

#include "ibs/core/timing/BarLines.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>

namespace ibs::core {

namespace {

constexpr double kPulseRoundingEpsilon = 1e-6;

std::string formatMs(double ms) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", ms);
    return buffer;
}

}  // namespace

BeatGrid::BeatGrid(const TimingData& timing, int divisionsPerWholeNote)
    : barStarts_(barStartsOf(timing)),
      lastBarLength_(barStarts_.size() >= 2 ? barStarts_.back() - barStarts_[barStarts_.size() - 2]
                                            : static_cast<Pulse>(timing.resolution) * 4),
      stepPulses_(divisionsPerWholeNote > 0 ? static_cast<double>(timing.resolution) * 4.0 / divisionsPerWholeNote
                                            : 0.0) {
    if (divisionsPerWholeNote <= 0) {
        throw std::invalid_argument("divisionsPerWholeNote must be positive");
    }
}

double BeatGrid::nearestGridPulse(double pulse) const {
    const auto next = std::upper_bound(barStarts_.begin(), barStarts_.end(), static_cast<Pulse>(std::floor(pulse)));
    Pulse barStart = next == barStarts_.begin() ? barStarts_.front() : *std::prev(next);
    Pulse barEnd = 0;
    if (next != barStarts_.end()) {
        barEnd = *next;
    } else {
        const Pulse barsPast = (static_cast<Pulse>(std::floor(pulse)) - barStart) / lastBarLength_;
        barStart += barsPast * lastBarLength_;
        barEnd = barStart + lastBarLength_;
    }
    // Why not 曲頭から等間隔に並べる: 変拍子の小節（7/8 など）を挟むと、それ以降のグリッドが小節頭からずれるため、小節ごとに数え直す
    const auto start = static_cast<double>(barStart);
    const double snapped = start + std::round((pulse - start) / stepPulses_) * stepPulses_;
    return std::min(static_cast<double>(barEnd), snapped);
}

CutPlan planCuts(const std::vector<Onset>& onsets, int sampleRate, Pulse placementStart, const TimingData& timing,
                 const QuantizeParameters& parameters) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sampleRate must be positive");
    }
    if (parameters.strength < 0.0 || parameters.strength > 1.0) {
        throw std::invalid_argument("strength must be in [0, 1]");
    }
    const TimingMap timingMap = timing.toTimingMap();
    const double placementSeconds = timingMap.secondsAt(placementStart);
    const std::optional<BeatGrid> grid =
        parameters.divisionsPerWholeNote > 0 ? std::optional<BeatGrid>(BeatGrid(timing, parameters.divisionsPerWholeNote))
                                             : std::nullopt;

    CutPlan plan;
    std::set<Pulse> usedOffsets;
    for (const auto& onset : onsets) {
        const double onsetSeconds = static_cast<double>(onset.sample) / sampleRate;
        const double songSeconds = placementSeconds + onsetSeconds;
        const double exactPulse = timingMap.pulseAt(songSeconds);

        bool snapped = false;
        Pulse pulse = 0;
        if (grid.has_value() && parameters.strength > 0.0) {
            const double gridPulse = grid->nearestGridPulse(exactPulse);
            const double gridSeconds = timingMap.secondsAt(std::llround(gridPulse));
            const bool withinWindow =
                !parameters.windowMs.has_value() || std::abs(gridSeconds - songSeconds) * 1000.0 <= *parameters.windowMs;
            if (withinWindow) {
                pulse = std::llround(exactPulse + parameters.strength * (gridPulse - exactPulse));
                snapped = true;
            }
        }
        if (!snapped) {
            // Why not 四捨五入する: 切り上がると切断点がアタックの後ろに来て、アタックが前のスライスに入ってしまうため切り捨てる
            pulse = static_cast<Pulse>(std::floor(exactPulse + kPulseRoundingEpsilon));
        }

        const Pulse offset = pulse - placementStart;
        if (offset <= 0) {
            ++plan.mergedIntoHead;
            continue;
        }
        if (!usedOffsets.insert(offset).second) {
            ++plan.mergedDuplicates;
            continue;
        }
        const double lateMs = (timingMap.secondsAt(pulse) - songSeconds) * 1000.0;
        plan.cuts.push_back(
            PlannedCut{offset, onsetSeconds, lateMs, snapped, lateMs > parameters.attackClipWarningMs});
    }
    return plan;
}

ApplyResult applyCutPlan(Project& project, PlacementId placementId, const CutPlan& plan) {
    ApplyResult result;
    std::vector<CutPointId> replaceable;
    for (const auto& cutPoint : project.cutPoints()) {
        if (cutPoint.placementId == placementId && cutPoint.origin == CutOrigin::Auto &&
            !project.hasNotes(cutPoint.id)) {
            replaceable.push_back(cutPoint.id);
        }
    }
    // Why not 自動検出の切断点をすべて消す: レーンを割り当て済みの切断点を消すと、譜面の作業が失われるため残す
    for (const CutPointId id : replaceable) {
        project.removeCutPoint(id);
        ++result.removed;
    }

    std::set<Pulse> existingOffsets;
    for (const auto& cutPoint : project.cutPoints()) {
        if (cutPoint.placementId == placementId) {
            existingOffsets.insert(cutPoint.offsetPulses);
        }
    }
    for (const auto& planned : plan.cuts) {
        if (existingOffsets.count(planned.offsetPulses) > 0) {
            ++result.keptExisting;
            continue;
        }
        project.addCutPoint(placementId, planned.offsetPulses, CutOrigin::Auto);
        ++result.added;
    }
    return result;
}

std::vector<Issue> attackClipIssues(const Project& project, PlacementId placementId, const CutPlan& plan) {
    const Placement& placement = project.placement(placementId);
    const std::string& assetName = project.asset(placement.assetId).name;
    std::vector<Issue> issues;
    for (const auto& planned : plan.cuts) {
        if (planned.attackClipped) {
            issues.push_back({Severity::Warning, "V09",
                              "'" + assetName + "' (配置 y=" + std::to_string(placement.startPulse) + ") の y=" +
                                  std::to_string(placement.startPulse + planned.offsetPulses) +
                                  " の切断点は、アタックより " + formatMs(planned.lateMs) + " ms 後ろにあります"});
        }
    }
    return issues;
}

}  // namespace ibs::core
