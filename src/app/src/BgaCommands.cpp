#include "ibs/app/BgaCommands.hpp"

#include <algorithm>

namespace ibs::app {

std::uint32_t addBgaImage(EditorDocument& document, const std::string& name) {
    const auto& headers = document.project().bga.headers;
    const auto existing = std::find_if(headers.begin(), headers.end(), [&](const core::BgaHeader& header) { return header.name == name; });
    if (existing != headers.end()) {
        return existing->id;
    }
    return document.apply("BGA 画像の追加", [&](core::Project& project) { return core::addBgaHeader(project.bga, name); });
}

void removeBgaImage(EditorDocument& document, std::uint32_t id) {
    document.apply("BGA 画像の削除", [&](core::Project& project) { core::removeBgaHeader(project.bga, id); });
}

void placeBgaEvent(EditorDocument& document, core::BgaTrack track, core::Pulse y, std::uint32_t id) {
    const auto& events = core::eventsOf(document.project().bga, track);
    if (std::any_of(events.begin(), events.end(), [&](const core::BgaEvent& event) { return event.y == y && event.id == id; })) {
        return;
    }
    document.apply("BGA の配置", [&](core::Project& project) { core::placeBgaEvent(project.bga, track, y, id); });
}

void removeBgaEvent(EditorDocument& document, core::BgaTrack track, core::Pulse y) {
    document.apply("BGA の削除", [&](core::Project& project) { core::removeBgaEvent(project.bga, track, y); });
}

void moveBgaEvent(EditorDocument& document, core::BgaTrack track, core::Pulse from, core::Pulse to) {
    if (from == to) {
        return;
    }
    document.apply("BGA の移動", [&](core::Project& project) { core::moveBgaEvent(project.bga, track, from, to); });
}

void updateSongInfo(EditorDocument& document, const core::SongInfo& info) {
    if (info == document.project().info) {
        return;
    }
    document.apply("曲の情報", [&](core::Project& project) { project.info = info; });
}

}  // namespace ibs::app
