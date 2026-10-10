#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ibs/core/model/Bga.hpp"
#include "ibs/core/model/Ids.hpp"
#include "ibs/core/model/SongInfo.hpp"
#include "ibs/core/model/TimingData.hpp"

namespace ibs::core {

inline constexpr int kBgmLane = 0;

struct Asset {
    AssetId id;
    std::string name;
};

struct Placement {
    PlacementId id;
    AssetId assetId;
    Pulse startPulse = 0;
    int channelGroup = 0;
};

enum class CutOrigin { Imported, Auto, Manual };

struct CutPoint {
    CutPointId id;
    PlacementId placementId;
    Pulse offsetPulses = 0;
    CutOrigin origin = CutOrigin::Manual;
};

struct Note {
    NoteId id;
    CutPointId cutPointId;
    int lane = 1;
    Pulse lengthPulses = 0;
};

class Project {
public:
    SongInfo info;
    TimingData timing;
    Bga bga;

    AssetId addAsset(std::string name);
    PlacementId addPlacement(AssetId assetId, Pulse startPulse, int channelGroup);
    CutPointId addCutPoint(PlacementId placementId, Pulse offsetPulses, CutOrigin origin);
    NoteId addNote(CutPointId cutPointId, int lane, Pulse lengthPulses);
    void removeCutPoint(CutPointId cutPointId);
    void moveCutPoint(CutPointId cutPointId, Pulse newOffsetPulses);
    void retimeCutPoint(CutPointId cutPointId, Pulse newOffsetPulses);
    void movePlacement(PlacementId placementId, Pulse newStartPulse);
    void removePlacement(PlacementId placementId);
    void removeNote(NoteId noteId);
    void setNoteLength(NoteId noteId, Pulse lengthPulses);
    void remapNoteLanes(const std::map<NoteId, int>& lanes);

    void markTempoAffected(PlacementId placementId);
    void clearTempoAffected(PlacementId placementId);
    [[nodiscard]] const std::set<PlacementId>& tempoAffectedPlacements() const noexcept { return tempoAffected_; }

    [[nodiscard]] bool hasNotes(CutPointId cutPointId) const;

    [[nodiscard]] const std::vector<Asset>& assets() const noexcept { return assets_; }
    [[nodiscard]] const std::vector<Placement>& placements() const noexcept { return placements_; }
    [[nodiscard]] const std::vector<CutPoint>& cutPoints() const noexcept { return cutPoints_; }
    [[nodiscard]] const std::vector<Note>& notes() const noexcept { return notes_; }

    [[nodiscard]] const Asset& asset(AssetId id) const;
    [[nodiscard]] const Placement& placement(PlacementId id) const;
    [[nodiscard]] const CutPoint& cutPoint(CutPointId id) const;
    [[nodiscard]] const Note& note(NoteId id) const;
    [[nodiscard]] bool containsCutPoint(CutPointId id) const;
    [[nodiscard]] bool containsNote(NoteId id) const;
    [[nodiscard]] std::vector<NoteId> notesOf(CutPointId cutPointId) const;
    [[nodiscard]] const Asset* findAssetByName(const std::string& name) const;

    [[nodiscard]] CutPointId headCutPointOf(PlacementId placementId) const;
    [[nodiscard]] std::vector<CutPointId> cutPointsOf(PlacementId placementId) const;
    [[nodiscard]] Pulse absolutePulseOf(const CutPoint& cutPoint) const;

private:
    std::uint64_t nextId_ = 1;
    std::vector<Asset> assets_;
    std::vector<Placement> placements_;
    std::vector<CutPoint> cutPoints_;
    std::vector<Note> notes_;
    std::set<PlacementId> tempoAffected_;
};

}  // namespace ibs::core
