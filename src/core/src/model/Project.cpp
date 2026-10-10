#include "ibs/core/model/Project.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace ibs::core {

namespace {

template <class Entity, class IdType>
const Entity& findById(const std::vector<Entity>& entities, IdType id, const char* kind) {
    const auto found =
        std::find_if(entities.begin(), entities.end(), [id](const Entity& entity) { return entity.id == id; });
    if (found == entities.end()) {
        throw std::invalid_argument(std::string(kind) + " not found: id=" + std::to_string(id.value));
    }
    return *found;
}

}  // namespace

AssetId Project::addAsset(std::string name) {
    if (findAssetByName(name) != nullptr) {
        throw std::invalid_argument("asset already exists: " + name);
    }
    const AssetId id{nextId_++};
    assets_.push_back(Asset{id, std::move(name)});
    return id;
}

PlacementId Project::addPlacement(AssetId assetId, Pulse startPulse, int channelGroup) {
    static_cast<void>(asset(assetId));
    if (startPulse < 0) {
        throw std::invalid_argument("placement start must not be negative");
    }
    const PlacementId id{nextId_++};
    placements_.push_back(Placement{id, assetId, startPulse, channelGroup});
    cutPoints_.push_back(CutPoint{CutPointId{nextId_++}, id, 0, CutOrigin::Imported});
    return id;
}

CutPointId Project::addCutPoint(PlacementId placementId, Pulse offsetPulses, CutOrigin origin) {
    static_cast<void>(placement(placementId));
    if (offsetPulses <= 0) {
        throw std::invalid_argument("cut point offset must be positive; offset 0 is the placement head");
    }
    const bool duplicated = std::any_of(cutPoints_.begin(), cutPoints_.end(), [&](const CutPoint& existing) {
        return existing.placementId == placementId && existing.offsetPulses == offsetPulses;
    });
    if (duplicated) {
        throw std::invalid_argument("cut point already exists at offset " + std::to_string(offsetPulses));
    }
    const CutPointId id{nextId_++};
    cutPoints_.push_back(CutPoint{id, placementId, offsetPulses, origin});
    return id;
}

NoteId Project::addNote(CutPointId cutPointId, int lane, Pulse lengthPulses) {
    static_cast<void>(cutPoint(cutPointId));
    if (lane <= kBgmLane) {
        throw std::invalid_argument("note lane must be positive; an unassigned cut point already plays as BGM");
    }
    if (lengthPulses < 0) {
        throw std::invalid_argument("note length must not be negative");
    }
    const bool duplicated = std::any_of(notes_.begin(), notes_.end(), [&](const Note& existing) {
        return existing.cutPointId == cutPointId && existing.lane == lane;
    });
    if (duplicated) {
        throw std::invalid_argument("note already exists on lane " + std::to_string(lane));
    }
    const NoteId id{nextId_++};
    notes_.push_back(Note{id, cutPointId, lane, lengthPulses});
    return id;
}

void Project::removeCutPoint(CutPointId cutPointId) {
    const CutPoint& target = cutPoint(cutPointId);
    if (target.offsetPulses == 0) {
        throw std::invalid_argument("the placement head cut point cannot be removed");
    }
    if (hasNotes(cutPointId)) {
        throw std::invalid_argument("cut point has notes; remove or move them first");
    }
    std::erase_if(cutPoints_, [cutPointId](const CutPoint& existing) { return existing.id == cutPointId; });
}

bool Project::hasNotes(CutPointId cutPointId) const {
    return std::any_of(notes_.begin(), notes_.end(), [cutPointId](const Note& note) { return note.cutPointId == cutPointId; });
}

const Asset& Project::asset(AssetId id) const { return findById(assets_, id, "asset"); }

const Placement& Project::placement(PlacementId id) const { return findById(placements_, id, "placement"); }

const CutPoint& Project::cutPoint(CutPointId id) const { return findById(cutPoints_, id, "cut point"); }

const Asset* Project::findAssetByName(const std::string& name) const {
    const auto found =
        std::find_if(assets_.begin(), assets_.end(), [&](const Asset& asset) { return asset.name == name; });
    return found == assets_.end() ? nullptr : &*found;
}

CutPointId Project::headCutPointOf(PlacementId placementId) const {
    const auto found = std::find_if(cutPoints_.begin(), cutPoints_.end(), [&](const CutPoint& cutPoint) {
        return cutPoint.placementId == placementId && cutPoint.offsetPulses == 0;
    });
    if (found == cutPoints_.end()) {
        throw std::invalid_argument("placement has no head cut point: id=" + std::to_string(placementId.value));
    }
    return found->id;
}

Pulse Project::absolutePulseOf(const CutPoint& cutPoint) const {
    return placement(cutPoint.placementId).startPulse + cutPoint.offsetPulses;
}

}  // namespace ibs::core
