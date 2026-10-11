#include "ibs/core/model/Project.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace ibs::core {

namespace {

// Why not 先頭から順に探す: 1 万ノーツ規模の譜面で描画や編集のたびに何度も引くため線形探索が効いてくる。
// id は単調に増える番号で末尾に追加し、削除しても順序は崩れないので、二分探索で引ける
template <class Container, class IdType>
auto locate(Container& entities, IdType id) {
    const auto found = std::lower_bound(entities.begin(), entities.end(), id,
                                        [](const auto& entity, IdType value) { return entity.id.value < value.value; });
    return found != entities.end() && found->id == id ? found : entities.end();
}

template <class Entity, class IdType>
const Entity& findById(const std::vector<Entity>& entities, IdType id, const char* kind) {
    const auto found = locate(entities, id);
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

void Project::retimeCutPoint(CutPointId cutPointId, Pulse newOffsetPulses) {
    const CutPoint& target = cutPoint(cutPointId);
    if (target.offsetPulses == 0) {
        throw std::invalid_argument("the placement head cut point cannot be moved; move the placement instead");
    }
    if (newOffsetPulses <= 0) {
        throw std::invalid_argument("cut point offset must be positive; offset 0 is the placement head");
    }
    const bool occupied = std::any_of(cutPoints_.begin(), cutPoints_.end(), [&](const CutPoint& existing) {
        return existing.placementId == target.placementId && existing.offsetPulses == newOffsetPulses &&
               existing.id != cutPointId;
    });
    if (occupied) {
        throw std::invalid_argument("cut point already exists at offset " + std::to_string(newOffsetPulses));
    }
    locate(cutPoints_, cutPointId)->offsetPulses = newOffsetPulses;
}

void Project::setCutOrigin(CutPointId cutPointId, CutOrigin origin) {
    static_cast<void>(cutPoint(cutPointId));
    locate(cutPoints_, cutPointId)->origin = origin;
}

void Project::moveCutPoint(CutPointId cutPointId, Pulse newOffsetPulses) {
    retimeCutPoint(cutPointId, newOffsetPulses);
    // Why not 自動検出の印のまま残す: 手で動かした切断点を再検出で消されないよう、手動の切断点として扱う
    locate(cutPoints_, cutPointId)->origin = CutOrigin::Manual;
}

void Project::movePlacement(PlacementId placementId, Pulse newStartPulse) {
    static_cast<void>(placement(placementId));
    if (newStartPulse < 0) {
        throw std::invalid_argument("placement start must not be negative");
    }
    auto& target = *locate(placements_, placementId);
    target.startPulse = newStartPulse;
}

void Project::removePlacement(PlacementId placementId) {
    static_cast<void>(placement(placementId));
    const auto removedCutPoints = cutPointsOf(placementId);
    std::erase_if(notes_, [&](const Note& note) {
        return std::find(removedCutPoints.begin(), removedCutPoints.end(), note.cutPointId) != removedCutPoints.end();
    });
    std::erase_if(cutPoints_, [placementId](const CutPoint& existing) { return existing.placementId == placementId; });
    std::erase_if(placements_, [placementId](const Placement& existing) { return existing.id == placementId; });
    tempoAffected_.erase(placementId);
}

void Project::removeNote(NoteId noteId) {
    static_cast<void>(note(noteId));
    notes_.erase(locate(notes_, noteId));
}

void Project::setNoteLength(NoteId noteId, Pulse lengthPulses) {
    static_cast<void>(note(noteId));
    if (lengthPulses < 0) {
        throw std::invalid_argument("note length must not be negative");
    }
    locate(notes_, noteId)->lengthPulses = lengthPulses;
}

// Why not 1 つずつ setNoteLane で変える: ミラーやレーンの入れ替えでは、途中の状態で同じ切断点の同じレーンに
// 2 つのノーツが一時的に重なり、正しい変更でも重複として拒否されるため、変更後の状態だけを検査する
void Project::remapNoteLanes(const std::map<NoteId, int>& lanes) {
    std::vector<Note> remapped = notes_;
    for (const auto& [id, lane] : lanes) {
        static_cast<void>(note(id));
        if (lane <= kBgmLane) {
            throw std::invalid_argument("note lane must be positive; remove the note to play the slice as BGM");
        }
        locate(remapped, id)->lane = lane;
    }
    std::set<std::pair<CutPointId, int>> occupied;
    for (const auto& note : remapped) {
        if (!occupied.emplace(note.cutPointId, note.lane).second) {
            throw std::invalid_argument("two notes would share lane " + std::to_string(note.lane) + " on one cut point");
        }
    }
    notes_ = std::move(remapped);
}

void Project::markTempoAffected(PlacementId placementId) {
    static_cast<void>(placement(placementId));
    tempoAffected_.insert(placementId);
}

void Project::clearTempoAffected(PlacementId placementId) { tempoAffected_.erase(placementId); }

std::vector<CutPointId> Project::cutPointsOf(PlacementId placementId) const {
    std::vector<std::pair<Pulse, CutPointId>> ordered;
    for (const auto& cutPoint : cutPoints_) {
        if (cutPoint.placementId == placementId) {
            ordered.emplace_back(cutPoint.offsetPulses, cutPoint.id);
        }
    }
    std::sort(ordered.begin(), ordered.end());
    std::vector<CutPointId> ids;
    for (const auto& [offset, id] : ordered) {
        ids.push_back(id);
    }
    return ids;
}

bool Project::hasNotes(CutPointId cutPointId) const {
    return std::any_of(notes_.begin(), notes_.end(), [cutPointId](const Note& note) { return note.cutPointId == cutPointId; });
}

const Asset& Project::asset(AssetId id) const { return findById(assets_, id, "asset"); }

const Placement& Project::placement(PlacementId id) const { return findById(placements_, id, "placement"); }

const CutPoint& Project::cutPoint(CutPointId id) const { return findById(cutPoints_, id, "cut point"); }

const Note& Project::note(NoteId id) const { return findById(notes_, id, "note"); }

bool Project::containsCutPoint(CutPointId id) const {
    return locate(cutPoints_, id) != cutPoints_.end();
}

bool Project::containsNote(NoteId id) const {
    return locate(notes_, id) != notes_.end();
}

std::vector<NoteId> Project::notesOf(CutPointId cutPointId) const {
    std::vector<NoteId> ids;
    for (const auto& existing : notes_) {
        if (existing.cutPointId == cutPointId) {
            ids.push_back(existing.id);
        }
    }
    return ids;
}

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
