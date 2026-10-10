#include "ibs/core/bmson/BmsonExporter.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <map>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ibs::core::bmson {

namespace {

struct ChannelBucket {
    std::string name;
    std::vector<Note> notes;
    Pulse lastPulse = std::numeric_limits<Pulse>::min();
};

using GroupKey = std::pair<int, std::size_t>;

std::unordered_map<CutPointId, std::vector<const ibs::core::Note*>> notesByCutPoint(const Project& project) {
    std::unordered_map<CutPointId, std::vector<const ibs::core::Note*>> grouped;
    for (const auto& note : project.notes()) {
        grouped[note.cutPointId].push_back(&note);
    }
    return grouped;
}

std::unordered_map<PlacementId, std::vector<const CutPoint*>> cutPointsByPlacement(const Project& project) {
    std::unordered_map<PlacementId, std::vector<const CutPoint*>> grouped;
    for (const auto& cutPoint : project.cutPoints()) {
        grouped[cutPoint.placementId].push_back(&cutPoint);
    }
    for (auto& [placementId, cutPoints] : grouped) {
        std::sort(cutPoints.begin(), cutPoints.end(),
                  [](const CutPoint* a, const CutPoint* b) { return a->offsetPulses < b->offsetPulses; });
    }
    return grouped;
}

std::size_t assetOrderOf(const Project& project, AssetId assetId) {
    const auto& assets = project.assets();
    const auto found =
        std::find_if(assets.begin(), assets.end(), [assetId](const Asset& asset) { return asset.id == assetId; });
    return static_cast<std::size_t>(std::distance(assets.begin(), found));
}

}  // namespace

Document exportDocument(const Project& project) {
    Document document;
    document.version = kSupportedVersion;
    document.info = project.info;
    document.timing = project.timing;
    document.bga = project.bga;

    const auto notesOfCutPoint = notesByCutPoint(project);
    const auto cutPointsOfPlacement = cutPointsByPlacement(project);

    std::map<GroupKey, std::vector<const Placement*>> placementsByGroup;
    for (const auto& placement : project.placements()) {
        placementsByGroup[{placement.channelGroup, assetOrderOf(project, placement.assetId)}].push_back(&placement);
    }

    for (auto& [groupKey, placements] : placementsByGroup) {
        std::sort(placements.begin(), placements.end(),
                  [](const Placement* a, const Placement* b) { return a->startPulse < b->startPulse; });

        std::vector<ChannelBucket> buckets;
        for (const Placement* placement : placements) {
            const auto& cutPoints = cutPointsOfPlacement.at(placement->id);
            const Pulse lastPulse = placement->startPulse + cutPoints.back()->offsetPulses;

            // Why not 常に同じチャンネルへ入れる: 前の配置の切断点が次の配置の開始より後ろにあると、
            // 読み込み直したときに後ろの配置の切断点として解釈され、スライスが変わってしまうため
            auto bucket = std::find_if(buckets.begin(), buckets.end(), [&](const ChannelBucket& candidate) {
                return candidate.lastPulse < placement->startPulse;
            });
            if (bucket == buckets.end()) {
                buckets.push_back(ChannelBucket{project.asset(placement->assetId).name, {}, lastPulse});
                bucket = std::prev(buckets.end());
            }

            for (const CutPoint* cutPoint : cutPoints) {
                const Pulse y = placement->startPulse + cutPoint->offsetPulses;
                const bool continues = cutPoint->offsetPulses != 0;
                const auto assigned = notesOfCutPoint.find(cutPoint->id);
                if (assigned == notesOfCutPoint.end()) {
                    bucket->notes.push_back(Note{kBgmLane, y, 0, continues});
                    continue;
                }
                for (const auto* note : assigned->second) {
                    bucket->notes.push_back(Note{note->lane, y, note->lengthPulses, continues});
                }
            }
            bucket->lastPulse = lastPulse;
        }

        for (auto& bucket : buckets) {
            std::sort(bucket.notes.begin(), bucket.notes.end(), [](const Note& a, const Note& b) {
                return std::tie(a.y, a.x) < std::tie(b.y, b.x);
            });
            document.soundChannels.push_back(SoundChannel{std::move(bucket.name), std::move(bucket.notes)});
        }
    }
    return document;
}

}  // namespace ibs::core::bmson
