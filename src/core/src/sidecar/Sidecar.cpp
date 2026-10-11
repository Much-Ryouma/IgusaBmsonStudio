#include "ibs/core/sidecar/Sidecar.hpp"

#include <set>

namespace ibs::core {

SidecarData captureSidecar(const Project& project, const std::map<PlacementId, SliceSettings>& sliceSettings) {
    SidecarData data;
    for (const auto& placement : project.placements()) {
        SidecarPlacement entry;
        entry.asset = project.asset(placement.assetId).name;
        entry.startPulse = placement.startPulse;
        entry.tempoAffected = project.tempoAffectedPlacements().contains(placement.id);
        // Why not すべての切断点を書く: bmson から読み直した切断点は「読み込み」になるので、それ以外のものだけを書けば足り、
        // 数千の切断点がある譜面でもサイドカーが小さく、差分も読みやすい
        for (const auto id : project.cutPointsOf(placement.id)) {
            const auto& cut = project.cutPoint(id);
            if (cut.origin != CutOrigin::Imported) {
                entry.cuts.push_back(SidecarCut{cut.offsetPulses, cut.origin});
            }
        }
        if (const auto found = sliceSettings.find(placement.id); found != sliceSettings.end()) {
            entry.slice = found->second;
        }
        data.placements.push_back(std::move(entry));
    }
    return data;
}

// Why not 編集用の ID をそのまま保存して照合する: bmson を開くたびに ID は振り直され、他のツールで bmson を
// 書き換えられることもあるため、bmson から必ず復元できる「素材名と配置の位置」「配置内の offset」で照合する
SidecarApplyResult applySidecar(Project& project, const SidecarData& sidecar) {
    SidecarApplyResult result;
    std::set<PlacementId> used;
    for (const auto& entry : sidecar.placements) {
        std::optional<PlacementId> match;
        for (const auto& placement : project.placements()) {
            if (!used.contains(placement.id) && placement.startPulse == entry.startPulse &&
                project.asset(placement.assetId).name == entry.asset) {
                match = placement.id;
                break;
            }
        }
        if (!match) {
            ++result.unmatchedPlacements;
            continue;
        }
        used.insert(*match);
        ++result.matchedPlacements;
        for (const auto id : project.cutPointsOf(*match)) {
            const Pulse offset = project.cutPoint(id).offsetPulses;
            for (const auto& cut : entry.cuts) {
                if (cut.offsetPulses == offset && cut.origin != project.cutPoint(id).origin) {
                    project.setCutOrigin(id, cut.origin);
                    ++result.restoredOrigins;
                }
            }
        }
        if (entry.tempoAffected) {
            project.markTempoAffected(*match);
        }
        if (entry.slice) {
            result.sliceSettings[*match] = *entry.slice;
        }
    }
    return result;
}

}  // namespace ibs::core
