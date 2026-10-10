#pragma once

#include <string>
#include <vector>

#include "ibs/core/model/Bga.hpp"
#include "ibs/core/model/SongInfo.hpp"
#include "ibs/core/model/TimingData.hpp"

namespace ibs::core::bmson {

inline constexpr const char* kSupportedVersion = "1.0.0";

struct Note {
    int x = 0;
    Pulse y = 0;
    Pulse l = 0;
    bool c = false;

    friend bool operator==(const Note&, const Note&) = default;
};

struct SoundChannel {
    std::string name;
    std::vector<Note> notes;

    friend bool operator==(const SoundChannel&, const SoundChannel&) = default;
};

struct Document {
    std::string version = kSupportedVersion;
    SongInfo info;
    TimingData timing;
    std::vector<SoundChannel> soundChannels;
    Bga bga;
};

}  // namespace ibs::core::bmson
