#include "ibs/core/bmson/BmsonImporter.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace ibs::core::bmson {

namespace {

std::string channelLabel(const SoundChannel& channel, std::size_t index) {
    return "sound_channels[" + std::to_string(index) + "] '" + channel.name + "'";
}

bool continuesAtPulse(const std::vector<const Note*>& notesAtPulse) {
    return std::all_of(notesAtPulse.begin(), notesAtPulse.end(), [](const Note* note) { return note->c; });
}

bool hasMixedContinuation(const std::vector<const Note*>& notesAtPulse) {
    const bool anyContinues =
        std::any_of(notesAtPulse.begin(), notesAtPulse.end(), [](const Note* note) { return note->c; });
    return anyContinues && !continuesAtPulse(notesAtPulse);
}

void importChannel(const SoundChannel& channel, std::size_t channelIndex, Project& project,
                   std::vector<Issue>& notices) {
    const std::string label = channelLabel(channel, channelIndex);

    std::map<Pulse, std::vector<const Note*>> notesByPulse;
    for (const Note& note : channel.notes) {
        if (note.x < 0 || note.y < 0) {
            notices.push_back({Severity::Warning, "IMP-NEGATIVE",
                               label + ": x=" + std::to_string(note.x) + " y=" + std::to_string(note.y) +
                                   " のノーツは位置が負のため読み込みません"});
            continue;
        }
        notesByPulse[note.y].push_back(&note);
    }
    if (notesByPulse.empty()) {
        notices.push_back({Severity::Info, "IMP-EMPTY-CHANNEL", label + ": ノーツがないため読み込みません"});
        return;
    }

    const Asset* existingAsset = project.findAssetByName(channel.name);
    const AssetId assetId = existingAsset != nullptr ? existingAsset->id : project.addAsset(channel.name);
    const int channelGroup = static_cast<int>(channelIndex);

    PlacementId currentPlacement{};
    Pulse currentStart = 0;

    for (const auto& [pulse, notesAtPulse] : notesByPulse) {
        if (hasMixedContinuation(notesAtPulse)) {
            notices.push_back({Severity::Info, "IMP-MIXED-C",
                               label + ": y=" + std::to_string(pulse) +
                                   " で c が混在しているため、仕様どおり c=false として扱います"});
        }

        CutPointId cutPointId;
        const bool restarts = !currentPlacement.isValid() || !continuesAtPulse(notesAtPulse);
        if (restarts) {
            if (!currentPlacement.isValid() && continuesAtPulse(notesAtPulse)) {
                notices.push_back({Severity::Info, "IMP-FIRST-CONTINUES",
                                   label + ": 最初のノーツ (y=" + std::to_string(pulse) +
                                       ") が c=true のため、ここを再生開始位置として扱います"});
            }
            currentPlacement = project.addPlacement(assetId, pulse, channelGroup);
            currentStart = pulse;
            cutPointId = project.headCutPointOf(currentPlacement);
        } else {
            cutPointId = project.addCutPoint(currentPlacement, pulse - currentStart, CutOrigin::Imported);
        }

        const bool assignedToLane =
            std::any_of(notesAtPulse.begin(), notesAtPulse.end(), [](const Note* note) { return note->x > kBgmLane; });
        std::set<int> lanesAdded;
        for (const Note* note : notesAtPulse) {
            if (note->x == kBgmLane) {
                if (assignedToLane) {
                    notices.push_back({Severity::Info, "IMP-BGM-SHADOWED",
                                       label + ": y=" + std::to_string(pulse) +
                                           " の BGM ノーツは、同じスライスがレーンに割り当て済みのため省略します"});
                }
                continue;
            }
            if (!lanesAdded.insert(note->x).second) {
                notices.push_back({Severity::Warning, "IMP-DUPLICATE-NOTE",
                                   label + ": x=" + std::to_string(note->x) + " y=" + std::to_string(pulse) +
                                       " のノーツが重複しているため、最初の1つだけ読み込みます"});
                continue;
            }
            project.addNote(cutPointId, note->x, note->l);
        }
    }
}

}  // namespace

ImportResult importDocument(const Document& document) {
    ImportResult result;
    Project& project = result.project;
    project.info = document.info;
    project.timing = document.timing;
    project.bga = document.bga;

    for (std::size_t index = 0; index < document.soundChannels.size(); ++index) {
        importChannel(document.soundChannels[index], index, project, result.notices);
    }
    return result;
}

}  // namespace ibs::core::bmson
