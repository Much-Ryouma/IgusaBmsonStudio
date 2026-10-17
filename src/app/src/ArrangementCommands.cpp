#include "ibs/app/ArrangementCommands.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>

#include "ibs/core/slicing/CutPlanner.hpp"

namespace ibs::app {

core::PlacementId placeAsset(EditorDocument& document, core::AssetId assetId, core::Pulse startPulse) {
    return document.apply("配置の追加", [&](core::Project& project) {
        return project.addPlacement(assetId, std::max<core::Pulse>(0, startPulse), 0);
    });
}

void movePlacement(EditorDocument& document, core::PlacementId placementId, core::Pulse startPulse) {
    if (document.project().placement(placementId).startPulse == startPulse) {
        return;
    }
    document.apply("配置の移動", [&](core::Project& project) { project.movePlacement(placementId, startPulse); });
}

core::PlacementId duplicatePlacement(EditorDocument& document, core::PlacementId placementId, core::Pulse startPulse) {
    return document.apply("配置の複製", [&](core::Project& project) {
        const core::Placement source = project.placement(placementId);
        const core::PlacementId copy = project.addPlacement(source.assetId, std::max<core::Pulse>(0, startPulse),
                                                            source.channelGroup);
        // Why not ノーツも複製する: 同じフレーズを別の場所に置くとき、レーンの割り当ては置き直す方が多く、
        // 複製したノーツが既存のノーツと重なると V05 の検証エラーになるため、切断点だけを写す
        for (const auto cutId : project.cutPointsOf(placementId)) {
            const core::CutPoint cut = project.cutPoint(cutId);
            if (cut.offsetPulses > 0) {
                project.addCutPoint(copy, cut.offsetPulses, cut.origin);
            }
        }
        return copy;
    });
}

void removePlacement(EditorDocument& document, core::PlacementId placementId) {
    document.apply("配置の削除", [&](core::Project& project) { project.removePlacement(placementId); });
}

void removeAsset(EditorDocument& document, core::AssetId assetId) {
    document.apply("素材の削除", [&](core::Project& project) { project.removeAsset(assetId); });
}

core::CutLayoutCopy copyCutLayout(EditorDocument& document, core::AssetId source, const std::vector<core::AssetId>& targets) {
    return document.apply("切断点のコピー", [&](core::Project& project) {
        core::CutLayoutCopy total;
        for (const auto target : targets) {
            const auto copied = core::copyCutLayout(project, source, target);
            total.placements += copied.placements;
            total.copiedCuts += copied.copiedCuts;
            total.removedNotes += copied.removedNotes;
        }
        return total;
    });
}

core::CutPointId addManualCut(EditorDocument& document, core::PlacementId placementId, core::Pulse songPulse) {
    const core::Pulse offset = songPulse - document.project().placement(placementId).startPulse;
    return document.apply("切断点の追加", [&](core::Project& project) {
        return project.addCutPoint(placementId, offset, core::CutOrigin::Manual);
    });
}

void moveCut(EditorDocument& document, core::CutPointId cutPointId, core::Pulse songPulse) {
    const core::CutPoint& cut = document.project().cutPoint(cutPointId);
    const core::Pulse offset = songPulse - document.project().placement(cut.placementId).startPulse;
    if (offset == cut.offsetPulses) {
        return;
    }
    document.apply("切断点の移動", [&](core::Project& project) { project.moveCutPoint(cutPointId, offset); });
}

std::size_t removeCuts(EditorDocument& document, const std::vector<core::CutPointId>& cutPointIds) {
    const auto& current = document.project();
    std::vector<core::CutPointId> removable;
    for (const auto id : cutPointIds) {
        const auto& cuts = current.cutPoints();
        const bool exists = std::any_of(cuts.begin(), cuts.end(), [id](const core::CutPoint& cut) { return cut.id == id; });
        // Why not 1 つでも消せなければ全体を中止する: 範囲選択には先頭やノーツ付きの切断点が混ざるのが普通なので、
        // 消せるものだけを消し、消せなかった数は呼び出し側で示す
        if (exists && current.cutPoint(id).offsetPulses > 0 && !current.hasNotes(id)) {
            removable.push_back(id);
        }
    }
    if (removable.empty()) {
        return 0;
    }
    document.apply("切断点の削除", [&](core::Project& project) {
        for (const auto id : removable) {
            project.removeCutPoint(id);
        }
    });
    return removable.size();
}

std::size_t snapCutsToGrid(EditorDocument& document, const std::vector<core::CutPointId>& cutPointIds,
                           int divisionsPerWholeNote) {
    const auto& current = document.project();
    const core::BeatGrid grid(current.timing, divisionsPerWholeNote);
    struct Move {
        core::CutPointId id;
        core::Pulse offset;
    };
    std::vector<Move> moves;
    std::set<std::pair<std::uint64_t, core::Pulse>> taken;
    for (const auto& cut : current.cutPoints()) {
        taken.emplace(cut.placementId.value, cut.offsetPulses);
    }
    for (const auto id : cutPointIds) {
        const auto& cuts = current.cutPoints();
        if (std::none_of(cuts.begin(), cuts.end(), [id](const core::CutPoint& cut) { return cut.id == id; })) {
            continue;
        }
        const auto& cut = current.cutPoint(id);
        const core::Pulse start = current.placement(cut.placementId).startPulse;
        const auto snapped = static_cast<core::Pulse>(
            std::llround(grid.nearestGridPulse(static_cast<double>(start + cut.offsetPulses))));
        const core::Pulse offset = snapped - start;
        // Why not 重なる切断点をまとめる: どちらにノーツが付いているかで残すべき方が変わるため、重なる移動は行わず手で直してもらう
        if (cut.offsetPulses == 0 || offset == cut.offsetPulses || offset <= 0 ||
            taken.contains({cut.placementId.value, offset})) {
            continue;
        }
        taken.erase({cut.placementId.value, cut.offsetPulses});
        taken.emplace(cut.placementId.value, offset);
        moves.push_back(Move{id, offset});
    }
    if (moves.empty()) {
        return 0;
    }
    document.apply("切断点のクオンタイズ", [&](core::Project& project) {
        for (const auto& move : moves) {
            project.moveCutPoint(move.id, move.offset);
        }
    });
    return moves.size();
}

}  // namespace ibs::app
