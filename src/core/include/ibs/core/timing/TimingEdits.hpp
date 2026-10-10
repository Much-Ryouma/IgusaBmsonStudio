#pragma once

#include <optional>
#include <vector>

#include "ibs/core/model/TimingData.hpp"
#include "ibs/core/timing/BarLines.hpp"

namespace ibs::core {

void setBpmChange(TimingData& timing, Pulse y, double bpm);
void removeBpmChange(TimingData& timing, Pulse y);
void setStop(TimingData& timing, Pulse y, Pulse durationPulses);
void removeStop(TimingData& timing, Pulse y);

struct BarLengthChange {
    int bar = 0;
    Pulse lengthPulses = 0;

    friend bool operator==(const BarLengthChange&, const BarLengthChange&) = default;
};

[[nodiscard]] Pulse barLengthOf(TimeSignature signature, int resolution);
[[nodiscard]] std::vector<BarLengthChange> barLengthChangesOf(const TimingData& timing);
[[nodiscard]] std::vector<Pulse> barLinesFromChanges(const std::vector<BarLengthChange>& changes, Pulse endPulse);
void setTimeSignature(TimingData& timing, int bar, TimeSignature signature, Pulse endPulse);
void removeTimeSignatureChange(TimingData& timing, int bar, Pulse endPulse);
void insertBarLine(TimingData& timing, Pulse y, Pulse endPulse);
void removeBarLine(TimingData& timing, Pulse y);

[[nodiscard]] int barIndexAt(const TimingData& timing, Pulse y);
[[nodiscard]] Pulse barStartPulse(const TimingData& timing, int bar);

}  // namespace ibs::core
