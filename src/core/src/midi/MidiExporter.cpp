#include "ibs/core/midi/MidiExporter.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

#include "ibs/core/asset/AssetPath.hpp"
#include "ibs/core/timing/BarLines.hpp"

namespace ibs::core {

namespace {

constexpr int kMaximumPpq = 0x7FFF;
constexpr int kMaximumTempoMicroseconds = 0xFFFFFF;
constexpr double kMicrosecondsPerMinute = 60'000'000.0;

class TickConverter {
public:
    TickConverter(int resolution, int ppq) : resolution_(resolution), ppq_(ppq) {}

    MidiTick operator()(Pulse pulse) {
        const Pulse scaled = pulse * ppq_;
        if (scaled % resolution_ != 0) {
            ++roundedCount_;
        }
        return (scaled * 2 + resolution_) / (static_cast<Pulse>(resolution_) * 2);
    }

    [[nodiscard]] int ppq() const noexcept { return ppq_; }
    [[nodiscard]] std::size_t roundedCount() const noexcept { return roundedCount_; }

private:
    int resolution_;
    int ppq_;
    std::size_t roundedCount_ = 0;
};

std::string trackNameOf(const std::string& assetName) {
    const std::string normalized = normalizeSeparators(assetName);
    const auto slash = normalized.find_last_of('/');
    std::string fileName = slash == std::string::npos ? normalized : normalized.substr(slash + 1);
    const auto dot = fileName.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? fileName : fileName.substr(0, dot);
}

std::vector<std::pair<Pulse, double>> tempoChanges(const TimingData& timing) {
    std::map<Pulse, double> byPulse{{0, timing.initialBpm}};
    auto events = timing.bpmEvents;
    std::stable_sort(events.begin(), events.end(), [](const BpmEvent& a, const BpmEvent& b) { return a.y < b.y; });
    for (const auto& event : events) {
        byPulse[event.y] = event.bpm;
    }
    return {byPulse.begin(), byPulse.end()};
}

int microsecondsPerQuarter(double bpm) {
    return static_cast<int>(std::clamp(std::llround(kMicrosecondsPerMinute / bpm), 1LL,
                                       static_cast<long long>(kMaximumTempoMicroseconds)));
}

void appendTempoEvents(MidiTrack& track, const TimingData& timing, Pulse origin, TickConverter& ticks) {
    const auto changes = tempoChanges(timing);
    double bpmAtOrigin = timing.initialBpm;
    for (const auto& [pulse, bpm] : changes) {
        if (pulse <= origin) {
            bpmAtOrigin = bpm;
        }
    }
    track.events.push_back({0, MidiEventKind::Tempo, 0, microsecondsPerQuarter(bpmAtOrigin), 0, {}});
    for (const auto& [pulse, bpm] : changes) {
        if (pulse > origin) {
            track.events.push_back({ticks(pulse - origin), MidiEventKind::Tempo, 0, microsecondsPerQuarter(bpm), 0, {}});
        }
    }
}

void appendTimeSignatures(MidiTrack& track, const TimingData& timing, TickConverter& ticks) {
    const auto starts = barStartsOf(timing);
    std::optional<TimeSignature> previous;
    if (starts.size() < 2) {
        track.events.push_back({0, MidiEventKind::TimeSignature, 0, 4, 4, {}});
        return;
    }
    for (std::size_t index = 0; index + 1 < starts.size(); ++index) {
        const auto signature = timeSignatureForBarLength(starts[index + 1] - starts[index], timing.resolution);
        if (signature.has_value() && signature != previous) {
            track.events.push_back({ticks(starts[index]), MidiEventKind::TimeSignature, 0, signature->numerator,
                                    signature->denominator, {}});
            previous = signature;
        }
    }
}

void appendStopMarkers(MidiTrack& track, const TimingData& timing, Pulse origin, TickConverter& ticks) {
    for (const auto& stop : timing.stopEvents) {
        if (stop.y >= origin) {
            track.events.push_back({ticks(stop.y - origin), MidiEventKind::Marker, 0, 0, 0,
                                    "STOP " + std::to_string(stop.durationPulses) + " pulses"});
        }
    }
}

MidiTrack conductorTrack(const TimingData& timing, const MidiProfile& profile, Pulse origin, bool withTimeSignature,
                         TickConverter& ticks) {
    MidiTrack track;
    appendTempoEvents(track, timing, origin, ticks);
    if (withTimeSignature) {
        appendTimeSignatures(track, timing, ticks);
    }
    if (profile.stopAsMarker) {
        appendStopMarkers(track, timing, origin, ticks);
    }
    return track;
}

MidiTrack noteTrack(const std::string& name, const std::vector<Pulse>& cutPulses, const TimingData& timing,
                    const MidiProfile& profile, Pulse origin, TickConverter& ticks) {
    MidiTrack track;
    track.name = name;
    track.events.push_back({0, MidiEventKind::TrackName, 0, 0, 0, name});
    if (profile.tempoInEveryTrack) {
        appendTempoEvents(track, timing, origin, ticks);
    }
    const int channel = profile.channel - 1;
    for (std::size_t index = 0; index < cutPulses.size(); ++index) {
        const MidiTick start = ticks(cutPulses[index] - origin);
        // Why not 最後の切断点を素材の終わりまで伸ばす: 素材の長さは音声を読まないと分からず、MIDI 出力だけのために音声を読みたくないため 1 拍にする
        const MidiTick end = index + 1 < cutPulses.size() ? ticks(cutPulses[index + 1] - origin) : start + ticks.ppq();
        track.events.push_back({start, MidiEventKind::NoteOn, channel, profile.noteNumber, profile.velocity, {}});
        track.events.push_back({std::max(end, start + 1), MidiEventKind::NoteOff, channel, profile.noteNumber, 0, {}});
    }
    return track;
}

std::vector<Pulse> sortedUnique(std::vector<Pulse> pulses) {
    std::sort(pulses.begin(), pulses.end());
    pulses.erase(std::unique(pulses.begin(), pulses.end()), pulses.end());
    return pulses;
}

int effectivePpq(const Project& project, const MidiProfile& profile) {
    const int ppq = profile.ppq.value_or(project.timing.resolution);
    if (ppq <= 0 || ppq > kMaximumPpq) {
        throw std::invalid_argument("ppq must be in 1.." + std::to_string(kMaximumPpq) + ", got " + std::to_string(ppq));
    }
    if (profile.channel < 1 || profile.channel > 16) {
        throw std::invalid_argument("MIDI channel must be in 1..16");
    }
    return ppq;
}

void appendCommonIssues(const Project& project, const TickConverter& ticks, std::vector<Issue>& issues) {
    if (!project.timing.stopEvents.empty()) {
        issues.push_back({Severity::Warning, "V13",
                          "STOP は MIDI で表現できません（" + std::to_string(project.timing.stopEvents.size()) +
                              " 個）。STOP 以降のノーツは、STOP の長さの分だけ早く鳴ります"});
    }
    if (ticks.roundedCount() > 0) {
        issues.push_back({Severity::Warning, "V14",
                          std::to_string(ticks.roundedCount()) + " 個の位置が PPQ " + std::to_string(ticks.ppq()) +
                              " に丸められました。PPQ を resolution (" + std::to_string(project.timing.resolution) +
                              ") の倍数にすると丸めずに出力できます"});
    }
}

}  // namespace

MidiExportResult buildCutPointMidi(const Project& project, const MidiProfile& profile) {
    TickConverter ticks(project.timing.resolution, effectivePpq(project, profile));
    MidiExportResult result;

    if (profile.origin == MidiTimeOrigin::SongStart) {
        MidiSequence sequence{ticks.ppq(), {}};
        if (profile.conductorTrack) {
            sequence.tracks.push_back(conductorTrack(project.timing, profile, 0, profile.timeSignature, ticks));
        }
        for (const auto& asset : project.assets()) {
            std::vector<Pulse> pulses;
            for (const auto& cutPoint : project.cutPoints()) {
                if (project.placement(cutPoint.placementId).assetId == asset.id) {
                    pulses.push_back(project.absolutePulseOf(cutPoint));
                }
            }
            if (!pulses.empty()) {
                sequence.tracks.push_back(
                    noteTrack(trackNameOf(asset.name), sortedUnique(std::move(pulses)), project.timing, profile, 0, ticks));
            }
        }
        result.files.push_back({project.info.title.empty() ? "cuts.mid" : project.info.title + ".mid", std::move(sequence)});
    } else {
        for (const auto& placement : project.placements()) {
            std::vector<Pulse> pulses;
            for (const auto& cutPoint : project.cutPoints()) {
                if (cutPoint.placementId == placement.id) {
                    pulses.push_back(placement.startPulse + cutPoint.offsetPulses);
                }
            }
            const std::string name = trackNameOf(project.asset(placement.assetId).name);
            MidiSequence sequence{ticks.ppq(), {}};
            // Why not 拍子を出力する: 配置の先頭は小節の頭とは限らず、小節線を 0 tick 基準にずらすと拍子の位置が意味を持たなくなるため
            if (profile.conductorTrack) {
                sequence.tracks.push_back(conductorTrack(project.timing, profile, placement.startPulse, false, ticks));
            }
            sequence.tracks.push_back(
                noteTrack(name, sortedUnique(std::move(pulses)), project.timing, profile, placement.startPulse, ticks));
            result.files.push_back({name + "_y" + std::to_string(placement.startPulse) + ".mid", std::move(sequence)});
        }
    }
    appendCommonIssues(project, ticks, result.issues);
    return result;
}

MidiExportResult buildTempoMapMidi(const Project& project, const MidiProfile& profile) {
    TickConverter ticks(project.timing.resolution, effectivePpq(project, profile));
    MidiTrack track = conductorTrack(project.timing, profile, 0, true, ticks);
    track.name = "Tempo";
    track.events.insert(track.events.begin(), {0, MidiEventKind::TrackName, 0, 0, 0, "Tempo"});

    MidiExportResult result;
    result.files.push_back({project.info.title.empty() ? "tempo.mid" : project.info.title + "_tempo.mid",
                            MidiSequence{ticks.ppq(), {std::move(track)}}});
    appendCommonIssues(project, ticks, result.issues);
    return result;
}

}  // namespace ibs::core
