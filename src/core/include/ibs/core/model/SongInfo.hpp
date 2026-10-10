#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ibs::core {

struct SongInfo {
    std::string title;
    std::string subtitle;
    std::string artist;
    std::vector<std::string> subartists;
    std::string genre;
    std::string modeHint = "beat-7k";
    std::string chartName;
    std::uint32_t level = 0;
    double judgeRank = 100.0;
    double total = 100.0;
    std::string backImage;
    std::string eyecatchImage;
    std::string titleImage;
    std::string bannerImage;
    std::string previewMusic;

    friend bool operator==(const SongInfo&, const SongInfo&) = default;
};

}  // namespace ibs::core
