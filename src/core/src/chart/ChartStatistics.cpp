#include "ibs/core/chart/ChartStatistics.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace ibs::core {

namespace {

constexpr double kSilenceDb = -120.0;

double toDb(double meanSquare) {
    return meanSquare > 0.0 ? std::max(kSilenceDb, 10.0 * std::log10(meanSquare)) : kSilenceDb;
}

bool isScratch(const KeyMode& mode, int lane) {
    return std::any_of(mode.lanes().begin(), mode.lanes().end(),
                       [lane](const LaneDefinition& definition) { return definition.x == lane && definition.kind == LaneKind::Scratch; });
}

}  // namespace

// Why not ノーツの数をそのまま数える: 同じレーン・同じ位置に重ねたノーツ（レイヤーノーツ）はプレーヤーでは 1 つとして
// 叩くので、V11 の総ノーツ数と同じく (レーン, 位置) を 1 つと数える
PlayableNoteStatistics playableNoteStatistics(const Project& project, const KeyMode& mode) {
    PlayableNoteStatistics statistics;
    const auto timing = project.timing.toTimingMap();
    std::set<std::pair<int, Pulse>> seen;
    for (const auto& note : project.notes()) {
        if (!mode.isPlayable(note.lane)) {
            continue;
        }
        const Pulse y = project.absolutePulseOf(project.cutPoint(note.cutPointId));
        if (!seen.emplace(note.lane, y).second) {
            continue;
        }
        ++statistics.total;
        ++statistics.perLane[note.lane];
        const bool scratch = isScratch(mode, note.lane);
        const bool longNote = note.lengthPulses > 0;
        if (scratch) {
            ++(longNote ? statistics.scratchLong : statistics.scratch);
        } else {
            ++(longNote ? statistics.longNotes : statistics.normal);
        }
        statistics.times.push_back(timing.secondsAt(y));
    }
    std::sort(statistics.times.begin(), statistics.times.end());
    return statistics;
}

BgmStatistics bgmStatistics(const Project& project, const KeyMode& mode) {
    BgmStatistics statistics;
    const auto timing = project.timing.toTimingMap();
    for (const auto& cut : project.cutPoints()) {
        const auto notes = project.notesOf(cut.id);
        const bool playable = std::any_of(notes.begin(), notes.end(), [&](NoteId id) { return mode.isPlayable(project.note(id).lane); });
        if (playable) {
            continue;
        }
        ++statistics.slices;
        statistics.bgmColumnNotes += notes.size();
        statistics.times.push_back(timing.secondsAt(project.absolutePulseOf(cut)));
    }
    std::sort(statistics.times.begin(), statistics.times.end());
    return statistics;
}

DensityStatistics densityOf(std::vector<double> times, double windowSeconds) {
    DensityStatistics density;
    std::sort(times.begin(), times.end());
    density.events = times.size();
    if (times.empty()) {
        return density;
    }
    density.firstSeconds = times.front();
    density.lastSeconds = times.back();
    std::size_t begin = 0;
    for (std::size_t end = 0; end < times.size(); ++end) {
        while (times[end] - times[begin] >= windowSeconds) {
            ++begin;
        }
        if (end - begin + 1 > density.maxPerSecond) {
            density.maxPerSecond = end - begin + 1;
            density.maxAtSeconds = times[begin];
        }
    }
    const double span = density.lastSeconds - density.firstSeconds;
    density.averagePerSecond = span > 0.0 ? static_cast<double>(times.size()) / span : static_cast<double>(times.size());
    return density;
}

LevelStatistics levelOf(const AudioBuffer& audio) {
    LevelStatistics level;
    const std::size_t frames = audio.frameCount();
    if (frames == 0 || audio.sampleRate <= 0) {
        return level;
    }
    level.durationSeconds = audio.durationSeconds();
    const auto channels = static_cast<std::size_t>(audio.channels);
    const auto second = static_cast<std::size_t>(audio.sampleRate);
    double total = 0.0;
    double peak = 0.0;
    std::size_t peakFrame = 0;
    std::vector<double> perFrame(frames, 0.0);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        double sum = 0.0;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double sample = audio.interleavedSamples[frame * channels + channel];
            sum += sample * sample;
            if (std::abs(sample) > peak) {
                peak = std::abs(sample);
                peakFrame = frame;
            }
        }
        perFrame[frame] = sum / static_cast<double>(channels);
        total += perFrame[frame];
    }
    level.averageDb = toDb(total / static_cast<double>(frames));
    level.peakDb = peak > 0.0 ? std::max(kSilenceDb, 20.0 * std::log10(peak)) : kSilenceDb;
    level.peakAtSeconds = static_cast<double>(peakFrame) / audio.sampleRate;
    double window = 0.0;
    double loudest = -1.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        window += perFrame[frame];
        if (frame >= second) {
            window -= perFrame[frame - second];
        }
        if (frame + 1 >= std::min(second, frames) && window > loudest) {
            loudest = window;
            level.loudestSecondAtSeconds = static_cast<double>(frame + 1 - std::min(second, frame + 1)) / audio.sampleRate;
        }
    }
    level.loudestSecondDb = toDb(loudest / static_cast<double>(std::min(second, frames)));
    return level;
}

}  // namespace ibs::core
