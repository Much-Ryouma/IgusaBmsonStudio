#include "ibs/infra/midi/StandardMidiFile.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::infra {

namespace {

using core::MidiEvent;
using core::MidiEventKind;

constexpr std::uint8_t kMetaEvent = 0xFF;
constexpr std::uint8_t kMetaTrackName = 0x03;
constexpr std::uint8_t kMetaMarker = 0x06;
constexpr std::uint8_t kMetaEndOfTrack = 0x2F;
constexpr std::uint8_t kMetaTempo = 0x51;
constexpr std::uint8_t kMetaTimeSignature = 0x58;
constexpr std::uint8_t kNoteOff = 0x80;
constexpr std::uint8_t kNoteOn = 0x90;

void appendBigEndian(std::string& out, std::uint32_t value, int bytes) {
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
        out.push_back(static_cast<char>((value >> shift) & 0xFF));
    }
}

void appendVariableLength(std::string& out, std::uint64_t value) { out += encodeVariableLength(value); }

void appendMeta(std::string& out, std::uint8_t type, const std::string& payload) {
    out.push_back(static_cast<char>(kMetaEvent));
    out.push_back(static_cast<char>(type));
    appendVariableLength(out, payload.size());
    out += payload;
}

int log2OfPowerOfTwo(int value) {
    int exponent = 0;
    while ((1 << exponent) < value) {
        ++exponent;
    }
    if ((1 << exponent) != value) {
        throw std::invalid_argument("time signature denominator must be a power of two");
    }
    return exponent;
}

void appendEvent(std::string& out, const MidiEvent& event) {
    switch (event.kind) {
        case MidiEventKind::TrackName: appendMeta(out, kMetaTrackName, event.text); break;
        case MidiEventKind::Marker: appendMeta(out, kMetaMarker, event.text); break;
        case MidiEventKind::Tempo: {
            std::string payload;
            appendBigEndian(payload, static_cast<std::uint32_t>(event.value1), 3);
            appendMeta(out, kMetaTempo, payload);
            break;
        }
        case MidiEventKind::TimeSignature: {
            std::string payload;
            payload.push_back(static_cast<char>(event.value1));
            payload.push_back(static_cast<char>(log2OfPowerOfTwo(event.value2)));
            payload.push_back(static_cast<char>(24));
            payload.push_back(static_cast<char>(8));
            appendMeta(out, kMetaTimeSignature, payload);
            break;
        }
        case MidiEventKind::NoteOn:
        case MidiEventKind::NoteOff: {
            const std::uint8_t status = event.kind == MidiEventKind::NoteOn ? kNoteOn : kNoteOff;
            out.push_back(static_cast<char>(status | (event.channel & 0x0F)));
            out.push_back(static_cast<char>(event.value1 & 0x7F));
            out.push_back(static_cast<char>(event.value2 & 0x7F));
            break;
        }
    }
}

std::string serializeTrack(const core::MidiTrack& track) {
    auto events = track.events;
    // Why not 追加した順のまま書く: 同じ音程が隙間なく続くとき、同じ tick で Note On が Note Off より先に来ると、
    // 受け取り側で直後の Off が新しいノーツを止めてしまうため、同じ tick では種類の順 (Off → On) に並べる
    std::stable_sort(events.begin(), events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        return a.tick != b.tick ? a.tick < b.tick : static_cast<int>(a.kind) < static_cast<int>(b.kind);
    });

    std::string body;
    core::MidiTick previous = 0;
    for (const auto& event : events) {
        appendVariableLength(body, static_cast<std::uint64_t>(event.tick - previous));
        appendEvent(body, event);
        previous = event.tick;
    }
    appendVariableLength(body, 0);
    appendMeta(body, kMetaEndOfTrack, {});

    std::string chunk = "MTrk";
    appendBigEndian(chunk, static_cast<std::uint32_t>(body.size()), 4);
    return chunk + body;
}

core::MidiTimeOrigin originFrom(const std::string& text) {
    if (text == "song_start") { return core::MidiTimeOrigin::SongStart; }
    if (text == "placement_start") { return core::MidiTimeOrigin::PlacementStart; }
    throw std::invalid_argument("unknown time_origin: " + text);
}

void requireValue(const nlohmann::json& root, const char* key, const std::string& expected) {
    if (root.contains(key) && root.at(key).get<std::string>() != expected) {
        throw std::invalid_argument(std::string(key) + " supports only \"" + expected + "\"");
    }
}

}  // namespace

std::string encodeVariableLength(std::uint64_t value) {
    std::uint8_t groups[10];
    int count = 0;
    do {
        groups[count++] = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
    } while (value > 0);
    std::string encoded;
    for (int index = count - 1; index >= 0; --index) {
        encoded.push_back(static_cast<char>(groups[index] | (index > 0 ? 0x80 : 0x00)));
    }
    return encoded;
}

std::string serializeStandardMidiFile(const core::MidiSequence& sequence) {
    if (sequence.ppq <= 0 || sequence.ppq > 0x7FFF) {
        throw std::invalid_argument("ppq must be in 1..32767");
    }
    std::string out = "MThd";
    appendBigEndian(out, 6, 4);
    appendBigEndian(out, 1, 2);
    appendBigEndian(out, static_cast<std::uint32_t>(sequence.tracks.size()), 2);
    appendBigEndian(out, static_cast<std::uint32_t>(sequence.ppq), 2);
    for (const auto& track : sequence.tracks) {
        out += serializeTrack(track);
    }
    return out;
}

core::MidiProfile parseMidiProfile(const std::string& jsonText) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(jsonText);
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("MIDI profile is not valid JSON: ") + error.what());
    }
    try {
        if (root.value("smf_format", 1) != 1) {
            throw std::invalid_argument("smf_format supports only 1");
        }
        requireValue(root, "track_per", "asset");
        requireValue(root, "track_name", "asset_basename");
        requireValue(root, "note_length", "until_next_cut");

        core::MidiProfile profile;
        profile.id = root.at("id").get<std::string>();
        profile.displayName = root.value("display_name", profile.id);
        if (root.contains("ppq") && root.at("ppq").is_number_integer()) {
            profile.ppq = root.at("ppq").get<int>();
        } else {
            requireValue(root, "ppq", "resolution");
        }
        profile.conductorTrack = root.value("conductor_track", true);
        profile.tempoInEveryTrack = root.value("tempo_in_every_track", false);
        profile.timeSignature = root.value("time_signature", true);
        profile.stopAsMarker = root.value("stop_as_marker", true);
        profile.channel = root.value("channel", 1);
        profile.noteNumber = root.value("note_number", 60);
        profile.velocity = root.value("velocity", 100);
        profile.origin = originFrom(root.value("time_origin", std::string("song_start")));

        if (profile.channel < 1 || profile.channel > 16 || profile.noteNumber < 0 || profile.noteNumber > 127 ||
            profile.velocity < 1 || profile.velocity > 127) {
            throw std::invalid_argument("channel must be 1..16, note_number 0..127, velocity 1..127");
        }
        if (!profile.conductorTrack && !profile.tempoInEveryTrack) {
            throw std::invalid_argument("either conductor_track or tempo_in_every_track must be true");
        }
        return profile;
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("MIDI profile is incomplete: ") + error.what());
    }
}

std::vector<core::MidiProfile> loadMidiProfiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    std::vector<core::MidiProfile> profiles;
    for (const auto& file : files) {
        profiles.push_back(parseMidiProfile(readTextFile(file)));
    }
    return profiles;
}

}  // namespace ibs::infra
