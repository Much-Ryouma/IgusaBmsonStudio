#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ibs/core/model/Project.hpp"
#include "ibs/core/slicing/CutPlanner.hpp"

namespace ibs::core {

inline constexpr int kSidecarVersion = 1;

struct SidecarCut {
    Pulse offsetPulses = 0;
    CutOrigin origin = CutOrigin::Imported;
};

struct SidecarPlacement {
    std::string asset;
    Pulse startPulse = 0;
    std::vector<SidecarCut> cuts;
    bool tempoAffected = false;
    std::optional<SliceSettings> slice;
};

struct SidecarData {
    int version = kSidecarVersion;
    std::vector<SidecarPlacement> placements;
};

struct SidecarApplyResult {
    std::map<PlacementId, SliceSettings> sliceSettings;
    std::size_t matchedPlacements = 0;
    std::size_t unmatchedPlacements = 0;
    std::size_t restoredOrigins = 0;
};

[[nodiscard]] SidecarData captureSidecar(const Project& project, const std::map<PlacementId, SliceSettings>& sliceSettings);
SidecarApplyResult applySidecar(Project& project, const SidecarData& sidecar);

}  // namespace ibs::core
