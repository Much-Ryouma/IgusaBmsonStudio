#include "ibs/core/chart/ChartEditing.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>

#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::core {

namespace {

constexpr double kDesyncToleranceSeconds = 0.0005;

}  // namespace

std::optional<NoteId> assignedNoteOf(const Project& project, CutPointId cutPointId) {
    const auto notes = project.notesOf(cutPointId);
    if (notes.empty()) {
        return std::nullopt;
    }
    return *std::min_element(notes.begin(), notes.end());
}

// Why not 押したレーンにノーツを追加し続ける: 割当モードはスライスの行き先を決める操作で、1 つのスライスを
// 複数レーンに重ねる（同時押し）ことはまれなため、既存の割当を移す。重ねたいときは別の操作で足す
NoteId assignLane(Project& project, CutPointId cutPointId, int lane) {
    if (lane <= kBgmLane) {
        throw std::invalid_argument("lane must be positive; use clearAssignment to play the slice as BGM");
    }
    for (const auto id : project.notesOf(cutPointId)) {
        if (project.note(id).lane == lane) {
            return id;
        }
    }
    if (const auto existing = assignedNoteOf(project, cutPointId)) {
        project.remapNoteLanes({{*existing, lane}});
        return *existing;
    }
    return project.addNote(cutPointId, lane, 0);
}

std::size_t clearAssignment(Project& project, CutPointId cutPointId) {
    const auto notes = project.notesOf(cutPointId);
    for (const auto id : notes) {
        project.removeNote(id);
    }
    return notes.size();
}

std::vector<CutPointId> cutPointsInSongOrder(const Project& project) {
    std::vector<std::tuple<Pulse, std::uint64_t, Pulse, CutPointId>> ordered;
    for (const auto& cut : project.cutPoints()) {
        const auto& placement = project.placement(cut.placementId);
        ordered.emplace_back(placement.startPulse + cut.offsetPulses, placement.assetId.value, placement.startPulse, cut.id);
    }
    std::sort(ordered.begin(), ordered.end());
    std::vector<CutPointId> ids;
    ids.reserve(ordered.size());
    for (const auto& entry : ordered) {
        ids.push_back(std::get<3>(entry));
    }
    return ids;
}

NotePattern copyPattern(const Project& project, const std::vector<NoteId>& notes) {
    NotePattern pattern;
    if (notes.empty()) {
        return pattern;
    }
    Pulse base = 0;
    bool first = true;
    for (const auto id : notes) {
        const Pulse y = project.absolutePulseOf(project.cutPoint(project.note(id).cutPointId));
        base = first ? y : std::min(base, y);
        first = false;
    }
    for (const auto id : notes) {
        const auto& note = project.note(id);
        const auto& cut = project.cutPoint(note.cutPointId);
        pattern.entries.push_back(NotePattern::Entry{project.absolutePulseOf(cut) - base, note.lane, note.lengthPulses,
                                                     project.placement(cut.placementId).assetId});
    }
    return pattern;
}

// Why not コピー元と同じ切断点の ID を探す: 貼り付け先は別の配置（同じフレーズを繰り返した箇所など）なので、
// 「同じ相対位置にある切断点」を探し、同じ素材のものを優先する
PasteResult pastePattern(Project& project, const NotePattern& pattern, Pulse targetPulse) {
    PasteResult result;
    for (const auto& entry : pattern.entries) {
        const Pulse y = targetPulse + entry.relativePulse;
        std::optional<CutPointId> sameAsset;
        std::optional<CutPointId> anyAsset;
        for (const auto& cut : project.cutPoints()) {
            if (project.absolutePulseOf(cut) != y) {
                continue;
            }
            const bool matches = project.placement(cut.placementId).assetId == entry.asset;
            if (matches && !sameAsset) {
                sameAsset = cut.id;
            } else if (!matches && !anyAsset) {
                anyAsset = cut.id;
            }
        }
        const auto target = sameAsset ? sameAsset : anyAsset;
        if (!target) {
            ++result.missingCutPoints;
            continue;
        }
        const auto notes = project.notesOf(*target);
        if (std::any_of(notes.begin(), notes.end(), [&](NoteId id) { return project.note(id).lane == entry.lane; })) {
            ++result.alreadyPresent;
            continue;
        }
        result.addedNotes.push_back(project.addNote(*target, entry.lane, entry.lengthPulses));
        ++result.added;
    }
    return result;
}

std::vector<int> keyLanesOf(const KeyMode& mode) {
    std::vector<int> lanes;
    for (const auto& lane : mode.lanes()) {
        if (lane.kind == LaneKind::White || lane.kind == LaneKind::Black) {
            lanes.push_back(lane.x);
        }
    }
    std::sort(lanes.begin(), lanes.end());
    return lanes;
}

std::map<NoteId, int> mirroredLanes(const Project& project, const std::vector<NoteId>& notes, const KeyMode& mode) {
    const auto keys = keyLanesOf(mode);
    std::map<NoteId, int> lanes;
    for (const auto id : notes) {
        const auto found = std::find(keys.begin(), keys.end(), project.note(id).lane);
        if (found != keys.end()) {
            lanes[id] = keys[keys.size() - 1 - static_cast<std::size_t>(std::distance(keys.begin(), found))];
        }
    }
    return lanes;
}

std::map<NoteId, int> swappedLanes(const Project& project, const std::vector<NoteId>& notes, int laneA, int laneB) {
    std::map<NoteId, int> lanes;
    for (const auto id : notes) {
        const int lane = project.note(id).lane;
        if (lane == laneA) {
            lanes[id] = laneB;
        } else if (lane == laneB) {
            lanes[id] = laneA;
        }
    }
    return lanes;
}

// Why not std::shuffle を使う: 並べ替えの手順は標準ライブラリの実装ごとに違い、同じシードでも
// MSVC と GCC で結果が変わるため、乱数列（mt19937 は規格で決まっている）から自前で並べ替える
std::map<NoteId, int> randomizedLanes(const Project& project, const std::vector<NoteId>& notes, const KeyMode& mode,
                                      std::uint32_t seed) {
    const auto keys = keyLanesOf(mode);
    auto shuffled = keys;
    std::mt19937 generator(seed);
    for (std::size_t index = shuffled.size(); index > 1; --index) {
        const auto pick = static_cast<std::size_t>(generator() % index);
        std::swap(shuffled[index - 1], shuffled[pick]);
    }
    std::map<NoteId, int> lanes;
    for (const auto id : notes) {
        const auto found = std::find(keys.begin(), keys.end(), project.note(id).lane);
        if (found != keys.end()) {
            lanes[id] = shuffled[static_cast<std::size_t>(std::distance(keys.begin(), found))];
        }
    }
    return lanes;
}

std::vector<PlacementId> placementsDesyncedBy(const Project& project, const TimingData& before) {
    const TimingMap oldMap = before.toTimingMap();
    const TimingMap newMap = project.timing.toTimingMap();
    std::vector<PlacementId> affected;
    for (const auto& placement : project.placements()) {
        const double oldStart = oldMap.secondsAt(placement.startPulse);
        const double newStart = newMap.secondsAt(placement.startPulse);
        for (const auto id : project.cutPointsOf(placement.id)) {
            const Pulse y = project.absolutePulseOf(project.cutPoint(id));
            const double drift = (newMap.secondsAt(y) - newStart) - (oldMap.secondsAt(y) - oldStart);
            if (std::abs(drift) > kDesyncToleranceSeconds) {
                affected.push_back(placement.id);
                break;
            }
        }
    }
    return affected;
}

// Why not 手で動かしたものとして扱う (moveCutPoint): 位置は変わるが音のどこで切るかは変わらないので、
// 自動検出の切断点は自動のまま残し、再検出で置き換えられるようにする
// Why not 切断点の pulse をそのまま残す: 音声ファイルは書き出したときのテンポで鳴るので、テンポを変えた後も
// 「配置の頭から何秒か」を保つ位置に動かさないと、スライスの頭が音の立ち上がりからずれる
std::size_t keepCutTimesAfterTempoChange(Project& project, PlacementId placementId, const TimingData& before) {
    const TimingMap oldMap = before.toTimingMap();
    const TimingMap newMap = project.timing.toTimingMap();
    const Pulse start = project.placement(placementId).startPulse;
    const double oldStart = oldMap.secondsAt(start);
    const double newStart = newMap.secondsAt(start);
    std::set<Pulse> taken;
    std::vector<std::pair<CutPointId, Pulse>> moves;
    for (const auto id : project.cutPointsOf(placementId)) {
        const auto& cut = project.cutPoint(id);
        if (cut.offsetPulses == 0) {
            taken.insert(0);
            continue;
        }
        const double seconds = oldMap.secondsAt(start + cut.offsetPulses) - oldStart;
        const auto pulse = static_cast<Pulse>(std::floor(newMap.pulseAt(newStart + seconds) + 1e-6));
        Pulse offset = std::max<Pulse>(1, pulse - start);
        while (taken.contains(offset)) {
            ++offset;
        }
        taken.insert(offset);
        moves.emplace_back(id, offset);
    }
    Pulse parking = 1;
    for (const auto& [id, offset] : moves) {
        parking = std::max({parking, offset + 1, project.cutPoint(id).offsetPulses + 1});
    }
    // Why not 前から順に最終位置へ動かす: 移動先に、まだ動かしていない切断点が残っていると重複として拒否されるため、
    // いったんすべてを重ならない位置へ退避してから最終位置に置く
    std::size_t moved = 0;
    for (std::size_t index = 0; index < moves.size(); ++index) {
        moved += moves[index].second != project.cutPoint(moves[index].first).offsetPulses ? 1u : 0u;
        project.retimeCutPoint(moves[index].first, parking + static_cast<Pulse>(index));
    }
    for (const auto& [id, offset] : moves) {
        project.retimeCutPoint(id, offset);
    }
    project.clearTempoAffected(placementId);
    return moved;
}

// Why not x はそのままにして x=0 のノーツを足す: 割当のない切断点はそれだけで BGM（x=0）として書き出されるので、
// BGM 列のノーツを消すだけで、x が 9 以上を扱えないプレーヤーでも同じ音になる
std::size_t foldBgmColumns(Project& project, const KeyMode& mode) {
    std::vector<NoteId> folded;
    for (const auto& note : project.notes()) {
        if (!mode.isPlayable(note.lane)) {
            folded.push_back(note.id);
        }
    }
    for (const auto id : folded) {
        project.removeNote(id);
    }
    return folded.size();
}

std::vector<std::size_t> playableNotesPerBar(const Project& project, const KeyMode& mode) {
    std::set<std::pair<int, Pulse>> positions;
    for (const auto& note : project.notes()) {
        if (mode.isPlayable(note.lane)) {
            positions.emplace(note.lane, project.absolutePulseOf(project.cutPoint(note.cutPointId)));
        }
    }
    std::vector<std::size_t> counts;
    for (const auto& [lane, y] : positions) {
        const auto bar = static_cast<std::size_t>(barIndexAt(project.timing, y));
        if (counts.size() <= bar) {
            counts.resize(bar + 1, 0);
        }
        ++counts[bar];
    }
    return counts;
}

// Why not total を固定値にする: ノーツ数が少ない譜面では固定値だとゲージが増えすぎ、多い譜面では足りなくなるため、
// BMS で広く使われている目安の式（7.605 × N ÷ (0.01 × N + 6.5)）を示す
double recommendedTotal(std::size_t playableNotes) {
    const auto notes = static_cast<double>(playableNotes);
    return 7.605 * notes / (0.01 * notes + 6.5);
}

}  // namespace ibs::core
