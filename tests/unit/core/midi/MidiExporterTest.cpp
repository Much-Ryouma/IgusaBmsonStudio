#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ibs/core/midi/MidiExporter.hpp"

namespace ibs::core {
namespace {

MidiProfile punaiProfile() {
    MidiProfile profile;
    profile.id = "punai-punai-ensou";
    profile.conductorTrack = false;
    profile.tempoInEveryTrack = true;
    profile.timeSignature = false;
    profile.stopAsMarker = false;
    return profile;
}

MidiProfile genericProfile() {
    MidiProfile profile;
    profile.id = "generic";
    return profile;
}

Project twoAssetProject() {
    Project project;
    project.timing.initialBpm = 120.0;
    project.timing.resolution = 240;
    const PlacementId kick = project.addPlacement(project.addAsset("drums/kick.wav"), 0, 0);
    project.addCutPoint(kick, 240, CutOrigin::Auto);
    project.addCutPoint(kick, 480, CutOrigin::Auto);
    project.addPlacement(project.addAsset("bass.wav"), 0, 1);
    return project;
}

std::vector<MidiEvent> eventsOf(const MidiTrack& track, MidiEventKind kind) {
    std::vector<MidiEvent> found;
    std::copy_if(track.events.begin(), track.events.end(), std::back_inserter(found),
                 [kind](const MidiEvent& event) { return event.kind == kind; });
    return found;
}

std::vector<MidiTick> ticksOf(const std::vector<MidiEvent>& events) {
    std::vector<MidiTick> ticks;
    for (const auto& event : events) {
        ticks.push_back(event.tick);
    }
    return ticks;
}

bool hasIssue(const MidiExportResult& result, const std::string& code) {
    return std::any_of(result.issues.begin(), result.issues.end(), [&](const Issue& issue) { return issue.code == code; });
}

TEST(MidiExporter, MID001_PunaiProfileWritesNameThenTempoAndGaplessNotes) {
    const auto result = buildCutPointMidi(twoAssetProject(), punaiProfile());

    ASSERT_EQ(result.files.size(), 1u);
    const auto& tracks = result.files[0].sequence.tracks;
    ASSERT_EQ(tracks.size(), 2u);
    const auto& kick = tracks[0];
    EXPECT_EQ(kick.events[0], (MidiEvent{0, MidiEventKind::TrackName, 0, 0, 0, "kick"}));
    EXPECT_EQ(kick.events[1], (MidiEvent{0, MidiEventKind::Tempo, 0, 500000, 0, {}}));
    EXPECT_EQ(ticksOf(eventsOf(kick, MidiEventKind::NoteOn)), (std::vector<MidiTick>{0, 240, 480}));
    EXPECT_EQ(ticksOf(eventsOf(kick, MidiEventKind::NoteOff)), (std::vector<MidiTick>{240, 480, 720}));
}

TEST(MidiExporter, MID002_GenericProfileUsesConductorTrack) {
    const auto result = buildCutPointMidi(twoAssetProject(), genericProfile());
    const auto& tracks = result.files[0].sequence.tracks;

    ASSERT_EQ(tracks.size(), 3u);
    EXPECT_EQ(eventsOf(tracks[0], MidiEventKind::Tempo).size(), 1u);
    EXPECT_EQ(eventsOf(tracks[0], MidiEventKind::TimeSignature).size(), 1u);
    EXPECT_TRUE(eventsOf(tracks[1], MidiEventKind::Tempo).empty());
}

TEST(MidiExporter, MID003_ConvertsToProfilePpqAndWarnsAboutRounding) {
    Project project;
    project.timing.resolution = 240;
    project.addCutPoint(project.addPlacement(project.addAsset("a.wav"), 0, 0), 80, CutOrigin::Auto);
    auto profile = punaiProfile();
    profile.ppq = 128;

    const auto result = buildCutPointMidi(project, profile);

    EXPECT_EQ(result.files[0].sequence.ppq, 128);
    EXPECT_EQ(ticksOf(eventsOf(result.files[0].sequence.tracks[0], MidiEventKind::NoteOn)),
              (std::vector<MidiTick>{0, 43}));
    EXPECT_TRUE(hasIssue(result, "V14"));
}

TEST(MidiExporter, MID004_PunaiProfileCopiesTempoChangesToEveryTrack) {
    auto project = twoAssetProject();
    project.timing.bpmEvents = {{960, 150.0}};

    const auto result = buildCutPointMidi(project, punaiProfile());
    const auto& tracks = result.files[0].sequence.tracks;

    for (const auto& track : tracks) {
        EXPECT_EQ(ticksOf(eventsOf(track, MidiEventKind::Tempo)), (std::vector<MidiTick>{0, 960})) << track.name;
    }
}

TEST(MidiExporter, MID005_WritesTimeSignatureOnlyWhereItChanges) {
    auto project = twoAssetProject();
    project.timing.barLines = std::vector<Pulse>{0, 960, 1800, 2760};

    const auto result = buildCutPointMidi(project, genericProfile());
    const auto& conductor = result.files[0].sequence.tracks[0];
    const auto signatures = eventsOf(conductor, MidiEventKind::TimeSignature);

    ASSERT_EQ(signatures.size(), 3u);
    EXPECT_EQ(signatures[0], (MidiEvent{0, MidiEventKind::TimeSignature, 0, 4, 4, {}}));
    EXPECT_EQ(signatures[1], (MidiEvent{960, MidiEventKind::TimeSignature, 0, 7, 8, {}}));
    EXPECT_EQ(signatures[2], (MidiEvent{1800, MidiEventKind::TimeSignature, 0, 4, 4, {}}));
}

TEST(MidiExporter, MID006_WarnsAboutStopsAndMarksThemOnlyWhenAsked) {
    auto project = twoAssetProject();
    project.timing.stopEvents = {{480, 240}};

    const auto generic = buildCutPointMidi(project, genericProfile());
    const auto punai = buildCutPointMidi(project, punaiProfile());

    EXPECT_TRUE(hasIssue(generic, "V13"));
    EXPECT_TRUE(hasIssue(punai, "V13"));
    EXPECT_EQ(eventsOf(generic.files[0].sequence.tracks[0], MidiEventKind::Marker).size(), 1u);
    for (const auto& track : punai.files[0].sequence.tracks) {
        EXPECT_TRUE(eventsOf(track, MidiEventKind::Marker).empty());
    }
}

TEST(MidiExporter, MID007_PlacementOriginWritesOneFilePerPlacement) {
    Project project;
    project.timing.resolution = 240;
    project.timing.initialBpm = 120.0;
    project.timing.bpmEvents = {{960, 150.0}};
    const AssetId fill = project.addAsset("fill.wav");
    project.addPlacement(fill, 0, 0);
    project.addCutPoint(project.addPlacement(fill, 1920, 0), 240, CutOrigin::Auto);
    auto profile = punaiProfile();
    profile.origin = MidiTimeOrigin::PlacementStart;

    const auto result = buildCutPointMidi(project, profile);

    ASSERT_EQ(result.files.size(), 2u);
    EXPECT_EQ(result.files[1].suggestedName, "fill_y1920.mid");
    const auto& track = result.files[1].sequence.tracks[0];
    EXPECT_EQ(ticksOf(eventsOf(track, MidiEventKind::NoteOn)), (std::vector<MidiTick>{0, 240}));
    EXPECT_EQ(eventsOf(track, MidiEventKind::Tempo)[0].value1, 400000);
}

TEST(MidiExporter, MID008_TempoOnlyHasSingleTrackWithoutNotes) {
    auto project = twoAssetProject();
    project.timing.bpmEvents = {{960, 150.0}};
    project.timing.barLines = std::vector<Pulse>{0, 960};

    const auto result = buildTempoMapMidi(project, genericProfile());

    ASSERT_EQ(result.files[0].sequence.tracks.size(), 1u);
    const auto& track = result.files[0].sequence.tracks[0];
    EXPECT_EQ(track.name, "Tempo");
    EXPECT_TRUE(eventsOf(track, MidiEventKind::NoteOn).empty());
    EXPECT_EQ(eventsOf(track, MidiEventKind::Tempo).size(), 2u);
}

TEST(MidiExporter, MID009_LastNoteLastsOneBeat) {
    Project project;
    project.timing.resolution = 240;
    project.addPlacement(project.addAsset("pad.wav"), 0, 0);

    const auto result = buildCutPointMidi(project, punaiProfile());
    const auto& track = result.files[0].sequence.tracks[0];

    EXPECT_EQ(ticksOf(eventsOf(track, MidiEventKind::NoteOff)), (std::vector<MidiTick>{240}));
}

}  // namespace
}  // namespace ibs::core
