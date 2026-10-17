#pragma once

#include <cstddef>
#include <vector>

#include "ibs/core/model/Project.hpp"

namespace ibs::core {

struct CutLayoutCopy {
    std::size_t placements = 0;
    std::size_t copiedCuts = 0;
    std::size_t removedNotes = 0;
};

[[nodiscard]] std::vector<Pulse> placementStartsOf(const Project& project, AssetId asset);
[[nodiscard]] std::vector<AssetId> assetsWithSamePlacements(const Project& project, AssetId source);
CutLayoutCopy copyCutLayout(Project& project, AssetId source, AssetId target);

}  // namespace ibs::core
