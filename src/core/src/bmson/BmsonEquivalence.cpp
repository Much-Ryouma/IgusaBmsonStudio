#include "ibs/core/bmson/BmsonEquivalence.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "ibs/core/model/Project.hpp"

namespace ibs::core::bmson {

namespace {

template <class T>
void compareField(std::vector<std::string>& differences, const char* name, const T& expected, const T& actual) {
    if (!(expected == actual)) {
        differences.push_back(std::string(name) + " が異なります");
    }
}

void compareInfo(std::vector<std::string>& differences, const SongInfo& expected, const SongInfo& actual) {
    compareField(differences, "info.title", expected.title, actual.title);
    compareField(differences, "info.subtitle", expected.subtitle, actual.subtitle);
    compareField(differences, "info.artist", expected.artist, actual.artist);
    compareField(differences, "info.subartists", expected.subartists, actual.subartists);
    compareField(differences, "info.genre", expected.genre, actual.genre);
    compareField(differences, "info.mode_hint", expected.modeHint, actual.modeHint);
    compareField(differences, "info.chart_name", expected.chartName, actual.chartName);
    compareField(differences, "info.level", expected.level, actual.level);
    compareField(differences, "info.judge_rank", expected.judgeRank, actual.judgeRank);
    compareField(differences, "info.total", expected.total, actual.total);
    compareField(differences, "info.back_image", expected.backImage, actual.backImage);
    compareField(differences, "info.eyecatch_image", expected.eyecatchImage, actual.eyecatchImage);
    compareField(differences, "info.title_image", expected.titleImage, actual.titleImage);
    compareField(differences, "info.banner_image", expected.bannerImage, actual.bannerImage);
    compareField(differences, "info.preview_music", expected.previewMusic, actual.previewMusic);
}

std::vector<BpmEvent> sortedByPulseKeepingOrder(std::vector<BpmEvent> events) {
    std::stable_sort(events.begin(), events.end(), [](const BpmEvent& a, const BpmEvent& b) { return a.y < b.y; });
    return events;
}

std::map<Pulse, Pulse> stopDurationByPulse(const std::vector<StopEvent>& events) {
    std::map<Pulse, Pulse> totals;
    for (const auto& event : events) {
        totals[event.y] += event.durationPulses;
    }
    return totals;
}

std::vector<Pulse> sortedLines(const std::optional<std::vector<Pulse>>& lines) {
    std::vector<Pulse> sorted = lines.value_or(std::vector<Pulse>{});
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

void compareTiming(std::vector<std::string>& differences, const TimingData& expected, const TimingData& actual) {
    compareField(differences, "info.init_bpm", expected.initialBpm, actual.initialBpm);
    compareField(differences, "info.resolution", expected.resolution, actual.resolution);
    compareField(differences, "bpm_events", sortedByPulseKeepingOrder(expected.bpmEvents),
                 sortedByPulseKeepingOrder(actual.bpmEvents));
    compareField(differences, "stop_events", stopDurationByPulse(expected.stopEvents),
                 stopDurationByPulse(actual.stopEvents));
    compareField(differences, "lines の有無", expected.barLines.has_value(), actual.barLines.has_value());
    compareField(differences, "lines", sortedLines(expected.barLines), sortedLines(actual.barLines));
}

std::vector<BgaEvent> sortedEvents(std::vector<BgaEvent> events) {
    std::sort(events.begin(), events.end(),
              [](const BgaEvent& a, const BgaEvent& b) { return std::tie(a.y, a.id) < std::tie(b.y, b.id); });
    return events;
}

void compareBga(std::vector<std::string>& differences, const Bga& expected, const Bga& actual) {
    auto byId = [](std::vector<BgaHeader> headers) {
        std::sort(headers.begin(), headers.end(),
                  [](const BgaHeader& a, const BgaHeader& b) { return std::tie(a.id, a.name) < std::tie(b.id, b.name); });
        return headers;
    };
    compareField(differences, "bga.bga_header", byId(expected.headers), byId(actual.headers));
    compareField(differences, "bga.bga_events", sortedEvents(expected.bgaEvents), sortedEvents(actual.bgaEvents));
    compareField(differences, "bga.layer_events", sortedEvents(expected.layerEvents), sortedEvents(actual.layerEvents));
    compareField(differences, "bga.poor_events", sortedEvents(expected.poorEvents), sortedEvents(actual.poorEvents));
}

std::vector<SoundChannel> normalizedChannels(const std::vector<SoundChannel>& channels) {
    std::vector<SoundChannel> normalized;
    for (const auto& channel : channels) {
        auto playback = normalizedForPlayback(channel);
        if (!playback.notes.empty()) {
            normalized.push_back(std::move(playback));
        }
    }
    std::sort(normalized.begin(), normalized.end(), [](const SoundChannel& a, const SoundChannel& b) {
        if (a.name != b.name) {
            return a.name < b.name;
        }
        return std::lexicographical_compare(
            a.notes.begin(), a.notes.end(), b.notes.begin(), b.notes.end(),
            [](const Note& l, const Note& r) { return std::tie(l.y, l.x, l.l, l.c) < std::tie(r.y, r.x, r.l, r.c); });
    });
    return normalized;
}

void compareChannels(std::vector<std::string>& differences, const std::vector<SoundChannel>& expected,
                     const std::vector<SoundChannel>& actual) {
    const auto left = normalizedChannels(expected);
    const auto right = normalizedChannels(actual);
    if (left.size() != right.size()) {
        differences.push_back("sound_channels の数が異なります (" + std::to_string(left.size()) + " → " +
                              std::to_string(right.size()) + ")");
        return;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].name != right[index].name) {
            differences.push_back("sound_channels の名前が異なります ('" + left[index].name + "' → '" +
                                  right[index].name + "')");
            continue;
        }
        if (left[index].notes != right[index].notes) {
            differences.push_back("sound_channels '" + left[index].name + "' のノーツが異なります (" +
                                  std::to_string(left[index].notes.size()) + " 個 → " +
                                  std::to_string(right[index].notes.size()) + " 個)");
        }
    }
}

}  // namespace

SoundChannel normalizedForPlayback(const SoundChannel& channel) {
    std::map<Pulse, std::vector<Note>> notesByPulse;
    for (const auto& note : channel.notes) {
        notesByPulse[note.y].push_back(note);
    }

    SoundChannel normalized{channel.name, {}};
    bool isFirstPulse = true;
    for (auto& [pulse, notes] : notesByPulse) {
        const bool continues =
            !isFirstPulse && std::all_of(notes.begin(), notes.end(), [](const Note& note) { return note.c; });
        isFirstPulse = false;

        const bool assignedToLane =
            std::any_of(notes.begin(), notes.end(), [](const Note& note) { return note.x > kBgmLane; });
        if (!assignedToLane) {
            normalized.notes.push_back(Note{kBgmLane, pulse, 0, continues});
            continue;
        }
        std::set<int> lanes;
        for (const auto& note : notes) {
            if (note.x > kBgmLane && lanes.insert(note.x).second) {
                normalized.notes.push_back(Note{note.x, pulse, note.l, continues});
            }
        }
    }
    std::sort(normalized.notes.begin(), normalized.notes.end(),
              [](const Note& a, const Note& b) { return std::tie(a.y, a.x) < std::tie(b.y, b.x); });
    return normalized;
}

std::vector<std::string> describePlaybackDifferences(const Document& expected, const Document& actual) {
    std::vector<std::string> differences;
    compareField(differences, "version", expected.version, actual.version);
    compareInfo(differences, expected.info, actual.info);
    compareTiming(differences, expected.timing, actual.timing);
    compareBga(differences, expected.bga, actual.bga);
    compareChannels(differences, expected.soundChannels, actual.soundChannels);
    return differences;
}

}  // namespace ibs::core::bmson
