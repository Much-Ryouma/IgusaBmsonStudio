#include "ibs/core/timing/TimingEdits.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ibs::core {

namespace {

Pulse defaultBarLength(const TimingData& timing) { return static_cast<Pulse>(std::max(1, timing.resolution)) * 4; }

std::vector<Pulse> explicitBarLines(const TimingData& timing, Pulse endPulse) {
    if (timing.barLines.has_value()) {
        return *timing.barLines;
    }
    // Why not lines を空のまま残す: 小節線を手で足すと、それより前の 4/4 の小節線が消えて 1 小節になってしまうため、
    // 編集を始める時点で、曲の終わりまでの既定の小節線を書き出しておく
    return barLinesFromChanges({{0, defaultBarLength(timing)}}, endPulse);
}

}  // namespace

void setBpmChange(TimingData& timing, Pulse y, double bpm) {
    if (!(bpm > 0.0)) {
        throw std::invalid_argument("BPM must be positive");
    }
    if (y < 0) {
        throw std::invalid_argument("BPM change position must not be negative");
    }
    if (y == 0) {
        timing.initialBpm = bpm;
        std::erase_if(timing.bpmEvents, [](const BpmEvent& event) { return event.y == 0; });
        return;
    }
    std::erase_if(timing.bpmEvents, [y](const BpmEvent& event) { return event.y == y; });
    timing.bpmEvents.push_back(BpmEvent{y, bpm});
    std::sort(timing.bpmEvents.begin(), timing.bpmEvents.end(),
              [](const BpmEvent& a, const BpmEvent& b) { return a.y < b.y; });
}

void removeBpmChange(TimingData& timing, Pulse y) {
    const auto removed = std::erase_if(timing.bpmEvents, [y](const BpmEvent& event) { return event.y == y; });
    if (removed == 0) {
        throw std::invalid_argument("no BPM change at y=" + std::to_string(y));
    }
}

void setStop(TimingData& timing, Pulse y, Pulse durationPulses) {
    if (durationPulses <= 0 || y < 0) {
        throw std::invalid_argument("STOP needs a non-negative position and a positive duration");
    }
    std::erase_if(timing.stopEvents, [y](const StopEvent& event) { return event.y == y; });
    timing.stopEvents.push_back(StopEvent{y, durationPulses});
    std::sort(timing.stopEvents.begin(), timing.stopEvents.end(),
              [](const StopEvent& a, const StopEvent& b) { return a.y < b.y; });
}

void removeStop(TimingData& timing, Pulse y) {
    const auto removed = std::erase_if(timing.stopEvents, [y](const StopEvent& event) { return event.y == y; });
    if (removed == 0) {
        throw std::invalid_argument("no STOP at y=" + std::to_string(y));
    }
}

Pulse barLengthOf(TimeSignature signature, int resolution) {
    if (signature.numerator <= 0 || signature.denominator <= 0 || resolution <= 0) {
        throw std::invalid_argument("time signature and resolution must be positive");
    }
    const Pulse scaled = static_cast<Pulse>(resolution) * 4 * signature.numerator;
    if (scaled % signature.denominator != 0) {
        throw std::invalid_argument("time signature " + std::to_string(signature.numerator) + "/" +
                                    std::to_string(signature.denominator) + " does not fit resolution " +
                                    std::to_string(resolution));
    }
    return scaled / signature.denominator;
}

std::vector<BarLengthChange> barLengthChangesOf(const TimingData& timing) {
    const auto starts = barStartsOf(timing);
    std::vector<BarLengthChange> changes;
    for (std::size_t index = 0; index + 1 < starts.size(); ++index) {
        const Pulse length = starts[index + 1] - starts[index];
        if (changes.empty() || changes.back().lengthPulses != length) {
            changes.push_back(BarLengthChange{static_cast<int>(index), length});
        }
    }
    if (changes.empty()) {
        changes.push_back(BarLengthChange{0, defaultBarLength(timing)});
    }
    return changes;
}

std::vector<Pulse> barLinesFromChanges(const std::vector<BarLengthChange>& changes, Pulse endPulse) {
    if (changes.empty() || changes.front().bar != 0) {
        throw std::invalid_argument("bar length changes must start at bar 0");
    }
    std::vector<Pulse> lines{0};
    std::size_t next = 1;
    Pulse length = changes.front().lengthPulses;
    for (int bar = 0; lines.back() <= endPulse; ++bar) {
        while (next < changes.size() && changes[next].bar <= bar) {
            length = changes[next].lengthPulses;
            ++next;
        }
        if (length <= 0) {
            throw std::invalid_argument("bar length must be positive");
        }
        lines.push_back(lines.back() + length);
    }
    return lines;
}

// Why not 指定した 1 小節だけを変える: DAW と同じく「その小節から次の拍子変更まで」を新しい拍子にしないと、
// 7/8 の区間を作るたびに全小節を指定し直すことになるため
void setTimeSignature(TimingData& timing, int bar, TimeSignature signature, Pulse endPulse) {
    if (bar < 0) {
        throw std::invalid_argument("bar must not be negative");
    }
    auto changes = barLengthChangesOf(timing);
    std::erase_if(changes, [bar](const BarLengthChange& change) { return change.bar == bar; });
    changes.push_back(BarLengthChange{bar, barLengthOf(signature, timing.resolution)});
    std::sort(changes.begin(), changes.end(), [](const auto& a, const auto& b) { return a.bar < b.bar; });
    if (changes.front().bar != 0) {
        changes.insert(changes.begin(), BarLengthChange{0, defaultBarLength(timing)});
    }
    const auto lastStart = barStartsOf(timing).back();
    timing.barLines = barLinesFromChanges(changes, std::max(endPulse, lastStart));
}

void removeTimeSignatureChange(TimingData& timing, int bar, Pulse endPulse) {
    if (bar == 0) {
        throw std::invalid_argument("the first time signature cannot be removed; change it instead");
    }
    auto changes = barLengthChangesOf(timing);
    const auto removed = std::erase_if(changes, [bar](const BarLengthChange& change) { return change.bar == bar; });
    if (removed == 0) {
        throw std::invalid_argument("no time signature change at bar " + std::to_string(bar));
    }
    const auto lastStart = barStartsOf(timing).back();
    timing.barLines = barLinesFromChanges(changes, std::max(endPulse, lastStart));
}

void insertBarLine(TimingData& timing, Pulse y, Pulse endPulse) {
    if (y <= 0) {
        throw std::invalid_argument("bar line position must be positive");
    }
    auto lines = explicitBarLines(timing, std::max(endPulse, y));
    if (std::find(lines.begin(), lines.end(), y) != lines.end()) {
        throw std::invalid_argument("bar line already exists at y=" + std::to_string(y));
    }
    lines.push_back(y);
    std::sort(lines.begin(), lines.end());
    timing.barLines = std::move(lines);
}

void removeBarLine(TimingData& timing, Pulse y) {
    if (y == 0) {
        throw std::invalid_argument("the bar line at the song start cannot be removed");
    }
    if (!timing.barLines.has_value() ||
        std::erase_if(*timing.barLines, [y](Pulse line) { return line == y; }) == 0) {
        throw std::invalid_argument("no bar line at y=" + std::to_string(y));
    }
}

int barIndexAt(const TimingData& timing, Pulse y) {
    const auto starts = barStartsOf(timing);
    const Pulse lastLength = starts.size() >= 2 ? starts.back() - starts[starts.size() - 2] : defaultBarLength(timing);
    const auto next = std::upper_bound(starts.begin(), starts.end(), y);
    const auto index = static_cast<int>(std::distance(starts.begin(), next)) - 1;
    if (next != starts.end() || y < starts.back()) {
        return std::max(0, index);
    }
    return index + static_cast<int>((y - starts.back()) / lastLength);
}

Pulse barStartPulse(const TimingData& timing, int bar) {
    const auto starts = barStartsOf(timing);
    if (bar < static_cast<int>(starts.size())) {
        return starts[static_cast<std::size_t>(std::max(0, bar))];
    }
    const Pulse lastLength = starts.size() >= 2 ? starts.back() - starts[starts.size() - 2] : defaultBarLength(timing);
    return starts.back() + static_cast<Pulse>(bar - static_cast<int>(starts.size()) + 1) * lastLength;
}

}  // namespace ibs::core
