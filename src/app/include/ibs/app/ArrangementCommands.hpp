#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ibs/app/EditorDocument.hpp"

namespace ibs::app {

core::PlacementId placeAsset(EditorDocument& document, core::AssetId assetId, core::Pulse startPulse);
void movePlacement(EditorDocument& document, core::PlacementId placementId, core::Pulse startPulse);
core::PlacementId duplicatePlacement(EditorDocument& document, core::PlacementId placementId, core::Pulse startPulse);
void removePlacement(EditorDocument& document, core::PlacementId placementId);

core::CutPointId addManualCut(EditorDocument& document, core::PlacementId placementId, core::Pulse songPulse);
void moveCut(EditorDocument& document, core::CutPointId cutPointId, core::Pulse songPulse);
std::size_t removeCuts(EditorDocument& document, const std::vector<core::CutPointId>& cutPointIds);
std::size_t snapCutsToGrid(EditorDocument& document, const std::vector<core::CutPointId>& cutPointIds,
                           int divisionsPerWholeNote);

}  // namespace ibs::app
