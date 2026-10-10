#include <stdexcept>

#include <gtest/gtest.h>

#include "../../support/Beat7k.hpp"
#include "ibs/core/chart/ChartEditing.hpp"
#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::core {
namespace {

struct Song {
    Project project;
    AssetId bass;
    AssetId lead;
    PlacementId bassPlacement;
    PlacementId leadPlacement;

    Song() {
        project.timing.initialBpm = 120.0;
        project.timing.resolution = 240;
        bass = project.addAsset("bass.wav");
        lead = project.addAsset("lead.wav");
        bassPlacement = project.addPlacement(bass, 0, 0);
        leadPlacement = project.addPlacement(lead, 0, 0);
    }

    CutPointId cut(PlacementId placement, Pulse offset) {
        return offset == 0 ? project.headCutPointOf(placement) : project.addCutPoint(placement, offset, CutOrigin::Auto);
    }
};

TEST(ChartEditing, CHE001_AssignMovesTheExistingNoteInsteadOfAddingAnother) {
    Song song;
    const auto cut = song.cut(song.bassPlacement, 240);

    const auto note = assignLane(song.project, cut, 3);
    const auto moved = assignLane(song.project, cut, 5);

    EXPECT_EQ(note, moved);
    EXPECT_EQ(song.project.note(note).lane, 5);
    EXPECT_EQ(song.project.notesOf(cut).size(), 1u);
    EXPECT_THROW(assignLane(song.project, cut, 0), std::invalid_argument);
}

TEST(ChartEditing, CHE002_ClearingPlaysTheSliceAsBgmAgain) {
    Song song;
    const auto cut = song.cut(song.bassPlacement, 240);
    song.project.addNote(cut, 1, 0);
    song.project.addNote(cut, 12, 0);

    EXPECT_EQ(clearAssignment(song.project, cut), 2u);
    EXPECT_TRUE(song.project.notesOf(cut).empty());
    EXPECT_FALSE(assignedNoteOf(song.project, cut).has_value());
}

TEST(ChartEditing, CHE003_OrdersCutPointsBySongPositionThenAsset) {
    Song song;
    const auto late = song.cut(song.bassPlacement, 480);
    const auto leadBeat = song.cut(song.leadPlacement, 240);
    const auto bassBeat = song.cut(song.bassPlacement, 240);

    const std::vector<CutPointId> expected{song.project.headCutPointOf(song.bassPlacement),
                                           song.project.headCutPointOf(song.leadPlacement), bassBeat, leadBeat, late};
    EXPECT_EQ(cutPointsInSongOrder(song.project), expected);
}

TEST(ChartEditing, CHE004_PastesPatternOntoCutsAtTheSameRelativePositions) {
    Song song;
    const auto source = song.project.addPlacement(song.bass, 1920, 0);
    song.project.addCutPoint(source, 240, CutOrigin::Auto);
    const auto copyFrom = std::vector<NoteId>{
        song.project.addNote(song.project.headCutPointOf(song.bassPlacement), 1, 0),
        song.project.addNote(song.cut(song.bassPlacement, 240), 3, 120),
        song.project.addNote(song.cut(song.bassPlacement, 360), 5, 0)};

    const auto pattern = copyPattern(song.project, copyFrom);
    const auto result = pastePattern(song.project, pattern, 1920);

    EXPECT_EQ(result.added, 2u);
    EXPECT_EQ(result.missingCutPoints, 1u);
    const auto pasted = song.project.note(result.addedNotes[1]);
    EXPECT_EQ(pasted.lane, 3);
    EXPECT_EQ(pasted.lengthPulses, 120);
    EXPECT_EQ(song.project.absolutePulseOf(song.project.cutPoint(pasted.cutPointId)), 2160);
    EXPECT_EQ(pastePattern(song.project, pattern, 1920).alreadyPresent, 2u);
}

TEST(ChartEditing, CHE005_PastePrefersTheSameAsset) {
    Song song;
    const auto bassNote = song.project.addNote(song.cut(song.bassPlacement, 240), 2, 0);
    song.cut(song.leadPlacement, 1200);
    const auto bassTarget = song.cut(song.bassPlacement, 1200);

    const auto result = pastePattern(song.project, copyPattern(song.project, {bassNote}), 1200);

    ASSERT_EQ(result.added, 1u);
    EXPECT_EQ(song.project.note(result.addedNotes[0]).cutPointId, bassTarget);
}

TEST(ChartEditing, CHE006_MirrorAndSwapOnlyTouchKeyLanes) {
    Song song;
    const KeyMode mode = ibs::testing::beat7k();
    const auto one = song.project.addNote(song.cut(song.bassPlacement, 240), 1, 0);
    const auto three = song.project.addNote(song.cut(song.bassPlacement, 480), 3, 0);
    const auto scratch = song.project.addNote(song.cut(song.bassPlacement, 720), 8, 0);
    const std::vector<NoteId> all{one, three, scratch};

    EXPECT_EQ(mirroredLanes(song.project, all, mode), (std::map<NoteId, int>{{one, 7}, {three, 5}}));
    EXPECT_EQ(swappedLanes(song.project, all, 3, 8), (std::map<NoteId, int>{{three, 8}, {scratch, 3}}));
}

TEST(ChartEditing, CHE007_RandomIsAPermutationFixedBySeed) {
    Song song;
    const KeyMode mode = ibs::testing::beat7k();
    std::vector<NoteId> notes;
    for (int lane = 1; lane <= 7; ++lane) {
        notes.push_back(song.project.addNote(song.cut(song.bassPlacement, 240 * lane), lane, 0));
    }

    const auto first = randomizedLanes(song.project, notes, mode, 42);
    const auto again = randomizedLanes(song.project, notes, mode, 42);

    EXPECT_EQ(first, again);
    std::set<int> lanes;
    for (const auto& [id, lane] : first) {
        lanes.insert(lane);
    }
    EXPECT_EQ(lanes, (std::set<int>{1, 2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(first.at(notes[0]), 4);
}

TEST(ChartEditing, CHE008_DetectsPlacementsWhoseCutsDriftAfterTempoChange) {
    Song song;
    song.cut(song.bassPlacement, 960);
    const auto later = song.project.addPlacement(song.bass, 3840, 0);
    song.project.addCutPoint(later, 240, CutOrigin::Auto);
    const TimingData before = song.project.timing;

    setBpmChange(song.project.timing, 1920, 60.0);

    EXPECT_EQ(placementsDesyncedBy(song.project, before), std::vector<PlacementId>{later});
}

TEST(ChartEditing, CHE009_KeepsCutTimesByMovingThemAndClearsTheMark) {
    Song song;
    const auto cut = song.cut(song.bassPlacement, 960);
    song.project.markTempoAffected(song.bassPlacement);
    const TimingData before = song.project.timing;
    setBpmChange(song.project.timing, 480, 60.0);

    const auto moved = keepCutTimesAfterTempoChange(song.project, song.bassPlacement, before);

    EXPECT_EQ(moved, 1u);
    EXPECT_EQ(song.project.cutPoint(cut).offsetPulses, 720);
    EXPECT_EQ(song.project.cutPoint(cut).origin, CutOrigin::Auto);
    EXPECT_TRUE(song.project.tempoAffectedPlacements().empty());
    EXPECT_TRUE(placementsDesyncedBy(song.project, before).size() <= 1u);
}

TEST(ChartEditing, CHE010_CountsPlayableNotesPerBarAndRecommendsTotal) {
    Song song;
    const KeyMode mode = ibs::testing::beat7k();
    song.project.addNote(song.cut(song.bassPlacement, 240), 1, 0);
    song.project.addNote(song.cut(song.leadPlacement, 240), 1, 0);
    song.project.addNote(song.cut(song.bassPlacement, 1200), 2, 0);
    song.project.addNote(song.cut(song.bassPlacement, 2000), 12, 0);

    EXPECT_EQ(playableNotesPerBar(song.project, mode), (std::vector<std::size_t>{1, 1}));
    EXPECT_NEAR(recommendedTotal(1000), 460.9, 0.1);
}

}  // namespace
}  // namespace ibs::core
