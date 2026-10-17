#include "ibs/core/chart/CutLayout.hpp"

#include <algorithm>
#include <stdexcept>

namespace ibs::core {

namespace {

std::vector<PlacementId> placementsInStartOrder(const Project& project, AssetId asset) {
    std::vector<const Placement*> owned;
    for (const auto& placement : project.placements()) {
        if (placement.assetId == asset) {
            owned.push_back(&placement);
        }
    }
    std::stable_sort(owned.begin(), owned.end(),
                     [](const Placement* a, const Placement* b) { return a->startPulse < b->startPulse; });
    std::vector<PlacementId> ids;
    for (const auto* placement : owned) {
        ids.push_back(placement->id);
    }
    return ids;
}

}  // namespace

std::vector<Pulse> placementStartsOf(const Project& project, AssetId asset) {
    std::vector<Pulse> starts;
    for (const auto id : placementsInStartOrder(project, asset)) {
        starts.push_back(project.placement(id).startPulse);
    }
    return starts;
}

std::vector<AssetId> assetsWithSamePlacements(const Project& project, AssetId source) {
    const auto starts = placementStartsOf(project, source);
    std::vector<AssetId> matches;
    if (starts.empty()) {
        return matches;
    }
    for (const auto& asset : project.assets()) {
        if (asset.id != source && placementStartsOf(project, asset.id) == starts) {
            matches.push_back(asset.id);
        }
    }
    return matches;
}

// Why not 割り当て（ノーツ）も写す: 同じ位置で鳴る別の素材のスライスに同じレーンを割り当てると、
// 1 回押すたびに 2 つの音が重なって鳴るため、写すのは切断点の位置だけにして、写し先の割り当ては外す
CutLayoutCopy copyCutLayout(Project& project, AssetId source, AssetId target) {
    if (source == target) {
        throw std::invalid_argument("copy source and target are the same asset");
    }
    const auto sourcePlacements = placementsInStartOrder(project, source);
    const auto targetPlacements = placementsInStartOrder(project, target);
    if (sourcePlacements.empty() || placementStartsOf(project, source) != placementStartsOf(project, target)) {
        throw std::invalid_argument("the two assets must be placed at exactly the same positions");
    }
    CutLayoutCopy result;
    for (std::size_t index = 0; index < sourcePlacements.size(); ++index) {
        const auto from = sourcePlacements[index];
        const auto to = targetPlacements[index];
        for (const auto cut : project.cutPointsOf(to)) {
            for (const auto note : project.notesOf(cut)) {
                project.removeNote(note);
                ++result.removedNotes;
            }
            if (project.cutPoint(cut).offsetPulses > 0) {
                project.removeCutPoint(cut);
            }
        }
        for (const auto cut : project.cutPointsOf(from)) {
            const auto& copied = project.cutPoint(cut);
            if (copied.offsetPulses > 0) {
                project.addCutPoint(to, copied.offsetPulses, copied.origin);
                ++result.copiedCuts;
            }
        }
        if (project.tempoAffectedPlacements().contains(from)) {
            project.markTempoAffected(to);
        } else {
            project.clearTempoAffected(to);
        }
        ++result.placements;
    }
    return result;
}

}  // namespace ibs::core
