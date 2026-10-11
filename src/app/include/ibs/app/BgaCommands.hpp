#pragma once

#include <cstdint>
#include <string>

#include "ibs/app/EditorDocument.hpp"
#include "ibs/core/model/BgaEditing.hpp"

namespace ibs::app {

std::uint32_t addBgaImage(EditorDocument& document, const std::string& name);
void removeBgaImage(EditorDocument& document, std::uint32_t id);
void placeBgaEvent(EditorDocument& document, core::BgaTrack track, core::Pulse y, std::uint32_t id);
void removeBgaEvent(EditorDocument& document, core::BgaTrack track, core::Pulse y);
void moveBgaEvent(EditorDocument& document, core::BgaTrack track, core::Pulse from, core::Pulse to);
void updateSongInfo(EditorDocument& document, const core::SongInfo& info);

}  // namespace ibs::app
