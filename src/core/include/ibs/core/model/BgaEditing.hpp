#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ibs/core/model/Bga.hpp"
#include "ibs/core/ports/FileSystemPort.hpp"

namespace ibs::core {

enum class BgaTrack { Bga, Layer, Poor };

[[nodiscard]] const std::vector<BgaEvent>& eventsOf(const Bga& bga, BgaTrack track);
std::uint32_t addBgaHeader(Bga& bga, const std::string& name);
void removeBgaHeader(Bga& bga, std::uint32_t id);
void placeBgaEvent(Bga& bga, BgaTrack track, Pulse y, std::uint32_t id);
void removeBgaEvent(Bga& bga, BgaTrack track, Pulse y);
void moveBgaEvent(Bga& bga, BgaTrack track, Pulse from, Pulse to);
[[nodiscard]] std::vector<std::string> unreachableBgaFiles(const Bga& bga, const FileSystemPort& chartFolder);

}  // namespace ibs::core
