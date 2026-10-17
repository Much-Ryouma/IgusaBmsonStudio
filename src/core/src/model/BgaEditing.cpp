#include "ibs/core/model/BgaEditing.hpp"

#include <algorithm>
#include <stdexcept>

#include "ibs/core/asset/AssetPath.hpp"

namespace ibs::core {

namespace {

std::vector<BgaEvent>& mutableEventsOf(Bga& bga, BgaTrack track) {
    switch (track) {
        case BgaTrack::Bga: return bga.bgaEvents;
        case BgaTrack::Layer: return bga.layerEvents;
        case BgaTrack::Poor: return bga.poorEvents;
    }
    return bga.bgaEvents;
}

bool hasHeader(const Bga& bga, std::uint32_t id) {
    return std::any_of(bga.headers.begin(), bga.headers.end(), [id](const BgaHeader& header) { return header.id == id; });
}

void sortByY(std::vector<BgaEvent>& events) {
    std::stable_sort(events.begin(), events.end(), [](const BgaEvent& a, const BgaEvent& b) { return a.y < b.y; });
}

}  // namespace

const std::vector<BgaEvent>& eventsOf(const Bga& bga, BgaTrack track) {
    return mutableEventsOf(const_cast<Bga&>(bga), track);
}

std::uint32_t addBgaHeader(Bga& bga, const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("BGA file name must not be empty");
    }
    for (const auto& header : bga.headers) {
        if (header.name == name) {
            return header.id;
        }
    }
    std::uint32_t id = 1;
    for (const auto& header : bga.headers) {
        id = std::max(id, header.id + 1);
    }
    bga.headers.push_back(BgaHeader{id, name});
    return id;
}

// Why not 使われている画像の削除を拒否する: 差し替えのために画像を消す場面では、イベントだけが残ると
// 存在しない id を指す壊れた譜面になるため、イベントも一緒に消す（Undo で戻せる）
void removeBgaHeader(Bga& bga, std::uint32_t id) {
    if (!hasHeader(bga, id)) {
        throw std::invalid_argument("no BGA header with id " + std::to_string(id));
    }
    std::erase_if(bga.headers, [id](const BgaHeader& header) { return header.id == id; });
    for (auto* events : {&bga.bgaEvents, &bga.layerEvents, &bga.poorEvents}) {
        std::erase_if(*events, [id](const BgaEvent& event) { return event.id == id; });
    }
}

void placeBgaEvent(Bga& bga, BgaTrack track, Pulse y, std::uint32_t id) {
    if (y < 0) {
        throw std::invalid_argument("BGA event position must not be negative");
    }
    if (!hasHeader(bga, id)) {
        throw std::invalid_argument("no BGA header with id " + std::to_string(id));
    }
    auto& events = mutableEventsOf(bga, track);
    std::erase_if(events, [y](const BgaEvent& event) { return event.y == y; });
    events.push_back(BgaEvent{y, id});
    sortByY(events);
}

void removeBgaEvent(Bga& bga, BgaTrack track, Pulse y) {
    if (std::erase_if(mutableEventsOf(bga, track), [y](const BgaEvent& event) { return event.y == y; }) == 0) {
        throw std::invalid_argument("no BGA event at y=" + std::to_string(y));
    }
}

void moveBgaEvent(Bga& bga, BgaTrack track, Pulse from, Pulse to) {
    auto& events = mutableEventsOf(bga, track);
    const auto found = std::find_if(events.begin(), events.end(), [from](const BgaEvent& event) { return event.y == from; });
    if (found == events.end()) {
        throw std::invalid_argument("no BGA event at y=" + std::to_string(from));
    }
    if (to < 0 || (to != from && std::any_of(events.begin(), events.end(), [to](const BgaEvent& event) { return event.y == to; }))) {
        throw std::invalid_argument("cannot move the BGA event to y=" + std::to_string(to));
    }
    found->y = to;
    sortByY(events);
}

std::vector<std::string> unreachableBgaFiles(const Bga& bga, const FileSystemPort& chartFolder) {
    std::vector<std::string> unreachable;
    for (const auto& header : bga.headers) {
        if (findUnsafePathReason(header.name) || !chartFolder.exists(normalizeSeparators(header.name))) {
            unreachable.push_back(header.name);
        }
    }
    return unreachable;
}

}  // namespace ibs::core
