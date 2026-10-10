#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ibs::core {

using MidiTick = std::int64_t;

enum class MidiTimeOrigin { SongStart, PlacementStart };

struct MidiProfile {
    std::string id;
    std::string displayName;
    std::optional<int> ppq;
    bool conductorTrack = true;
    bool tempoInEveryTrack = false;
    bool timeSignature = true;
    bool stopAsMarker = true;
    int channel = 1;
    int noteNumber = 60;
    int velocity = 100;
    MidiTimeOrigin origin = MidiTimeOrigin::SongStart;
};

enum class MidiEventKind { TrackName, Tempo, TimeSignature, Marker, NoteOff, NoteOn };

struct MidiEvent {
    MidiTick tick = 0;
    MidiEventKind kind = MidiEventKind::Marker;
    int channel = 0;
    int value1 = 0;
    int value2 = 0;
    std::string text;

    friend bool operator==(const MidiEvent&, const MidiEvent&) = default;
};

struct MidiTrack {
    std::string name;
    std::vector<MidiEvent> events;
};

struct MidiSequence {
    int ppq = 480;
    std::vector<MidiTrack> tracks;
};

}  // namespace ibs::core
