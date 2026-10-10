#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ibs/core/model/Project.hpp"
#include "ibs/core/slicing/OnsetDetector.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::core {

struct QuantizeParameters {
    int divisionsPerWholeNote = 16;
    double strength = 1.0;
    std::optional<double> windowMs;
    // Why not 3 ms: 完成品のステムで測ると、作者が実際に採用した切断点の多くが 3〜7 ms の範囲に入り、警告が多すぎて役に立たなかった
    double attackClipWarningMs = 10.0;
};

class BeatGrid {
public:
    BeatGrid(const TimingData& timing, int divisionsPerWholeNote);

    [[nodiscard]] double nearestGridPulse(double pulse) const;

private:
    std::vector<Pulse> barStarts_;
    Pulse lastBarLength_;
    double stepPulses_;
};

struct PlannedCut {
    Pulse offsetPulses = 0;
    double onsetSeconds = 0.0;
    double lateMs = 0.0;
    bool snapped = false;
    bool attackClipped = false;
};

struct CutPlan {
    std::vector<PlannedCut> cuts;
    std::size_t mergedIntoHead = 0;
    std::size_t mergedDuplicates = 0;
};

[[nodiscard]] CutPlan planCuts(const std::vector<Onset>& onsets, int sampleRate, Pulse placementStart,
                               const TimingData& timing, const QuantizeParameters& parameters);

struct ApplyResult {
    std::size_t added = 0;
    std::size_t removed = 0;
    std::size_t keptExisting = 0;
};

ApplyResult applyCutPlan(Project& project, PlacementId placementId, const CutPlan& plan);

[[nodiscard]] std::vector<Issue> attackClipIssues(const Project& project, PlacementId placementId,
                                                  const CutPlan& plan);

}  // namespace ibs::core
