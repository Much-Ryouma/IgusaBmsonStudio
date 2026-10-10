#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/midi/StandardMidiFile.hpp"

namespace ibs::infra {
namespace {

using core::MidiEvent;
using core::MidiEventKind;

std::string bytes(std::initializer_list<int> values) {
    std::string out;
    for (const int value : values) {
        out.push_back(static_cast<char>(value));
    }
    return out;
}

core::MidiSequence singleTrack(std::vector<MidiEvent> events, int ppq = 96) {
    return core::MidiSequence{ppq, {core::MidiTrack{"t", std::move(events)}}};
}

TEST(StandardMidiFile, SMF001_WritesFormat1Header) {
    const auto file = serializeStandardMidiFile(core::MidiSequence{96, {core::MidiTrack{}, core::MidiTrack{}}});

    EXPECT_EQ(file.substr(0, 14), bytes({'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0, 96}));
}

TEST(StandardMidiFile, SMF002_EncodesVariableLengthQuantities) {
    EXPECT_EQ(encodeVariableLength(0), bytes({0x00}));
    EXPECT_EQ(encodeVariableLength(127), bytes({0x7F}));
    EXPECT_EQ(encodeVariableLength(128), bytes({0x81, 0x00}));
    EXPECT_EQ(encodeVariableLength(16383), bytes({0xFF, 0x7F}));
    EXPECT_EQ(encodeVariableLength(16384), bytes({0x81, 0x80, 0x00}));
}

TEST(StandardMidiFile, SMF003_WritesNoteOffBeforeNoteOnAtSameTick) {
    const auto file = serializeStandardMidiFile(singleTrack({{10, MidiEventKind::NoteOn, 0, 60, 100, {}},
                                                             {10, MidiEventKind::NoteOff, 0, 60, 0, {}}}));

    EXPECT_LT(file.find(bytes({0x80, 60, 0})), file.find(bytes({0x90, 60, 100})));
}

TEST(StandardMidiFile, SMF004_WritesTempoAndTimeSignatureMetaEvents) {
    const auto file = serializeStandardMidiFile(singleTrack(
        {{0, MidiEventKind::Tempo, 0, 600000, 0, {}}, {0, MidiEventKind::TimeSignature, 0, 7, 8, {}}}));

    EXPECT_NE(file.find(bytes({0xFF, 0x51, 0x03, 0x09, 0x27, 0xC0})), std::string::npos);
    EXPECT_NE(file.find(bytes({0xFF, 0x58, 0x04, 0x07, 0x03, 0x18, 0x08})), std::string::npos);
}

TEST(StandardMidiFile, SMF005_EndsEveryTrackWithEndOfTrack) {
    const auto file = serializeStandardMidiFile(singleTrack({}));

    EXPECT_EQ(file.substr(file.size() - 4), bytes({0x00, 0xFF, 0x2F, 0x00}));
}

TEST(MidiProfileLoader, MPF001_LoadsBundledPunaiProfile) {
    const auto profile = parseMidiProfile(
        readTextFile(std::filesystem::path(IBS_SOURCE_DIR) / "midi_profiles" / "punai-punai-ensou.json"));

    EXPECT_FALSE(profile.conductorTrack);
    EXPECT_TRUE(profile.tempoInEveryTrack);
    EXPECT_FALSE(profile.timeSignature);
    EXPECT_FALSE(profile.ppq.has_value());
    EXPECT_EQ(profile.origin, core::MidiTimeOrigin::SongStart);
}

TEST(MidiProfileLoader, MPF002_RejectsChannelOutOfRange) {
    EXPECT_THROW(static_cast<void>(parseMidiProfile(R"({"id": "p", "channel": 17})")), std::invalid_argument);
}

TEST(MidiProfileLoader, MPF003_RejectsUnsupportedTrackGrouping) {
    EXPECT_THROW(static_cast<void>(parseMidiProfile(R"({"id": "p", "track_per": "channel"})")), std::invalid_argument);
}

TEST(MidiProfileLoader, MPF004_RejectsProfileWithoutTempo) {
    EXPECT_THROW(static_cast<void>(parseMidiProfile(
                     R"({"id": "p", "conductor_track": false, "tempo_in_every_track": false})")),
                 std::invalid_argument);
}

}  // namespace
}  // namespace ibs::infra
