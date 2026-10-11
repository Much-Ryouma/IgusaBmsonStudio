#include "ibs/core/validation/Validator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "ibs/core/asset/AssetPath.hpp"
#include "ibs/core/chart/ChartEditing.hpp"

namespace ibs::core {

namespace {

using LanePosition = std::pair<int, Pulse>;

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
}

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isLaneChecked(int lane, const KeyMode* keyMode) {
    return keyMode == nullptr ? lane > kBgmLane : keyMode->isPlayable(lane);
}

std::map<LanePosition, std::set<Pulse>> lengthsByLanePosition(const Project& project, const KeyMode* keyMode) {
    std::map<LanePosition, std::set<Pulse>> lengths;
    for (const auto& note : project.notes()) {
        if (!isLaneChecked(note.lane, keyMode)) {
            continue;
        }
        const Pulse y = project.absolutePulseOf(project.cutPoint(note.cutPointId));
        lengths[{note.lane, y}].insert(note.lengthPulses);
    }
    return lengths;
}

void checkTiming(const Project& project, std::vector<Issue>& issues) {
    if (!(project.timing.initialBpm > 0.0)) {
        issues.push_back({Severity::Error, "V01", "init_bpm が 0 以下です"});
    }
    for (const auto& event : project.timing.bpmEvents) {
        if (!(event.bpm > 0.0)) {
            issues.push_back({Severity::Error, "V01", "y=" + std::to_string(event.y) + " の BPM が 0 以下です"});
        }
    }
    if (project.timing.resolution <= 0) {
        issues.push_back({Severity::Error, "V02", "resolution が 0 以下です"});
    }
}

void checkFileReference(const std::string& field, const std::string& path, bool isSound, Severity missingSeverity,
                        const FileSystemPort* fileSystem, std::vector<Issue>& issues) {
    if (const auto unsafe = findUnsafePathReason(path)) {
        issues.push_back({Severity::Error, "V03", field + " '" + path + "': " + *unsafe});
        return;
    }
    if (fileSystem == nullptr) {
        return;
    }
    const bool found = isSound ? resolveSoundFile(path, *fileSystem).has_value()
                               : fileSystem->exists(normalizeSeparators(path));
    if (!found) {
        issues.push_back({missingSeverity, "V04", field + " '" + path + "' が見つかりません"});
    }
}

void checkFiles(const Project& project, const FileSystemPort* fileSystem, std::vector<Issue>& issues) {
    for (const auto& asset : project.assets()) {
        checkFileReference("音素材", asset.name, true, Severity::Error, fileSystem, issues);
        if (endsWith(toLower(asset.name), ".mp3")) {
            issues.push_back({Severity::Warning, "V07",
                              "音素材 '" + asset.name + "' は mp3 です。先頭に無音が入り、タイミングがずれることがあります"});
        }
    }
    for (const auto& header : project.bga.headers) {
        checkFileReference("BGA (id=" + std::to_string(header.id) + ")", header.name, false, Severity::Warning,
                           fileSystem, issues);
    }
    const std::pair<const char*, const std::string*> images[] = {
        {"back_image", &project.info.backImage},       {"eyecatch_image", &project.info.eyecatchImage},
        {"title_image", &project.info.titleImage},     {"banner_image", &project.info.bannerImage},
        {"preview_music", &project.info.previewMusic},
    };
    for (const auto& [field, value] : images) {
        if (!value->empty()) {
            checkFileReference(field, *value, false, Severity::Warning, fileSystem, issues);
        }
    }
}

void checkNotes(const Project& project, const KeyMode* keyMode, std::vector<Issue>& issues) {
    const auto lengths = lengthsByLanePosition(project, keyMode);

    std::map<int, std::vector<std::pair<Pulse, Pulse>>> notesByLane;
    for (const auto& [position, lengthSet] : lengths) {
        const auto& [lane, y] = position;
        if (lengthSet.size() > 1) {
            issues.push_back({Severity::Error, "V06",
                              "レーン " + std::to_string(lane) + " y=" + std::to_string(y) +
                                  " の重なったノーツ（レイヤーノーツ）の長さが一致しません"});
        }
        notesByLane[lane].emplace_back(y, *lengthSet.rbegin());
    }

    for (const auto& [lane, laneNotes] : notesByLane) {
        for (std::size_t index = 0; index + 1 < laneNotes.size(); ++index) {
            const auto& [y, length] = laneNotes[index];
            const Pulse nextY = laneNotes[index + 1].first;
            if (length > 0 && nextY <= y + length) {
                issues.push_back({Severity::Error, "V05",
                                  "レーン " + std::to_string(lane) + " の y=" + std::to_string(y) +
                                      " のロングノーツが y=" + std::to_string(nextY) + " のノーツと重なっています"});
            }
        }
    }
}

void checkSharedSlices(const Project& project, const KeyMode* keyMode, std::vector<Issue>& issues) {
    std::map<std::uint64_t, std::vector<int>> lanesByCut;
    for (const auto& note : project.notes()) {
        const bool playable = keyMode != nullptr ? keyMode->isPlayable(note.lane) : note.lane > kBgmLane;
        if (playable) {
            lanesByCut[note.cutPointId.value].push_back(note.lane);
        }
    }
    for (const auto& [cutValue, lanes] : lanesByCut) {
        if (lanes.size() < 2) {
            continue;
        }
        const auto& cut = project.cutPoint(CutPointId{cutValue});
        std::string laneText;
        for (const int lane : lanes) {
            laneText += (laneText.empty() ? "" : ", ") + std::to_string(lane);
        }
        issues.push_back({Severity::Warning, "V15",
                          "'" + project.asset(project.placement(cut.placementId).assetId).name + "' の y=" +
                              std::to_string(project.absolutePulseOf(cut)) + " のスライスが複数のレーン（" + laneText +
                              "）に割り当てられています。同時に押すと音が重なり、Pulsus では音切りの区切りが崩れます"});
    }
}

}  // namespace

std::size_t countPlayableNotes(const Project& project, const KeyMode& keyMode) {
    return lengthsByLanePosition(project, &keyMode).size();
}

std::vector<Issue> validateProject(const Project& project, const ValidationContext& context) {
    std::vector<Issue> issues;
    if (context.keyMode == nullptr) {
        issues.push_back({Severity::Warning, "V00",
                          "mode_hint '" + project.info.modeHint +
                              "' のモード定義がないため、x が 1 以上のノーツをすべてプレイアブルとして検証します"});
    }
    checkTiming(project, issues);
    checkFiles(project, context.fileSystem, issues);
    checkNotes(project, context.keyMode, issues);
    checkSharedSlices(project, context.keyMode, issues);
    if (context.bannerImageSize && context.bannerImageSize->width * 4 != context.bannerImageSize->height * 15) {
        issues.push_back({Severity::Warning, "V10",
                          "banner_image が 15:4 ではありません（" + std::to_string(context.bannerImageSize->width) + "×" +
                              std::to_string(context.bannerImageSize->height) + "。推奨は 300×80 など）"});
    }
    for (const auto placementId : project.tempoAffectedPlacements()) {
        const auto& placement = project.placement(placementId);
        issues.push_back({Severity::Warning, "V12",
                          "'" + project.asset(placement.assetId).name + "' (配置 y=" + std::to_string(placement.startPulse) +
                              ") は切断点を作った後に BPM・STOP が変わったため、切断点と音の立ち上がりがずれています"});
    }
    if (context.keyMode != nullptr) {
        const auto notes = countPlayableNotes(project, *context.keyMode);
        issues.push_back({Severity::Info, "V11",
                          "総ノーツ数: " + std::to_string(notes) + "（total の目安: " +
                              std::to_string(static_cast<long long>(std::lround(recommendedTotal(notes)))) + "）"});
    }
    return issues;
}

}  // namespace ibs::core
