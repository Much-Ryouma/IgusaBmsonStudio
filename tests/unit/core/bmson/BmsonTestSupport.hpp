#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "ibs/core/bmson/BmsonDocument.hpp"

namespace ibs::core::bmson::testing {

inline Document documentWithChannels(std::initializer_list<SoundChannel> channels) {
    Document document;
    document.info.title = "test";
    document.timing.initialBpm = 120.0;
    document.timing.resolution = 240;
    document.soundChannels = channels;
    return document;
}

inline SoundChannel channel(std::string name, std::vector<Note> notes) {
    return SoundChannel{std::move(name), std::move(notes)};
}

}  // namespace ibs::core::bmson::testing
