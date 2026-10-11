#include <algorithm>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "../../support/Beat7k.hpp"
#include "../../support/FakeFileSystem.hpp"
#include "ibs/core/validation/Validator.hpp"

namespace ibs::core {
namespace {

using ibs::testing::FakeFileSystem;

std::optional<Issue> findIssue(const std::vector<Issue>& issues, const std::string& code) {
    const auto found =
        std::find_if(issues.begin(), issues.end(), [&](const Issue& issue) { return issue.code == code; });
    return found == issues.end() ? std::nullopt : std::optional<Issue>(*found);
}

class ValidatorTest : public ::testing::Test {
protected:
    Project project;
    KeyMode mode = ibs::testing::beat7k();
    FakeFileSystem fileSystem{{"a.wav", "b.wav"}};

    std::vector<Issue> validate() { return validateProject(project, ValidationContext{&mode, &fileSystem, {}}); }

    CutPointId cutPointAt(const std::string& asset, Pulse y) {
        const Asset* existing = project.findAssetByName(asset);
        const AssetId assetId = existing != nullptr ? existing->id : project.addAsset(asset);
        const PlacementId placement = project.addPlacement(assetId, y, static_cast<int>(project.placements().size()));
        return project.headCutPointOf(placement);
    }
};

TEST_F(ValidatorTest, VAL001_ReportsNonPositiveInitialBpm) {
    project.timing.initialBpm = 0.0;

    const auto issue = findIssue(validate(), "V01");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Error);
}

TEST_F(ValidatorTest, VAL002_ReportsNonPositiveResolution) {
    project.timing.resolution = 0;

    const auto issue = findIssue(validate(), "V02");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Error);
}

TEST_F(ValidatorTest, VAL003_ReportsUnsafeAssetPath) {
    project.addAsset("../a.wav");

    const auto issue = findIssue(validate(), "V03");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Error);
}

TEST_F(ValidatorTest, VAL004_ReportsMissingSoundAsError) {
    project.addAsset("missing.wav");

    const auto issue = findIssue(validate(), "V04");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Error);
}

TEST_F(ValidatorTest, VAL005_ReportsMissingBgaAsWarning) {
    project.bga.headers.push_back({1, "missing.png"});

    const auto issue = findIssue(validate(), "V04");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Warning);
}

TEST_F(ValidatorTest, VAL006_ReportsLongNoteEndingOnNextNote) {
    project.addNote(cutPointAt("a.wav", 0), 1, 480);
    project.addNote(cutPointAt("b.wav", 480), 1, 0);

    EXPECT_TRUE(findIssue(validate(), "V05").has_value());
}

TEST_F(ValidatorTest, VAL007_AcceptsNoteRightAfterLongNoteEnd) {
    project.addNote(cutPointAt("a.wav", 0), 1, 480);
    project.addNote(cutPointAt("b.wav", 481), 1, 0);

    EXPECT_FALSE(findIssue(validate(), "V05").has_value());
}

TEST_F(ValidatorTest, VAL008_ReportsLayeredNotesWithDifferentLengths) {
    project.addNote(cutPointAt("a.wav", 0), 3, 0);
    project.addNote(cutPointAt("b.wav", 0), 3, 240);

    EXPECT_TRUE(findIssue(validate(), "V06").has_value());
}

TEST_F(ValidatorTest, VAL009_WarnsAboutMp3) {
    project.addAsset("a.mp3");

    const auto issue = findIssue(validate(), "V07");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Warning);
}

TEST_F(ValidatorTest, VAL010_IgnoresOverlapOnBgmColumns) {
    project.addNote(cutPointAt("a.wav", 0), 12, 480);
    project.addNote(cutPointAt("b.wav", 240), 12, 0);

    EXPECT_FALSE(findIssue(validate(), "V05").has_value());
}

TEST_F(ValidatorTest, VAL011_CountsLayeredNotesOnceAndSkipsBgmColumns) {
    project.addNote(cutPointAt("a.wav", 0), 1, 0);
    project.addNote(cutPointAt("b.wav", 0), 1, 0);
    project.addNote(cutPointAt("a.wav", 240), 2, 0);
    project.addNote(cutPointAt("b.wav", 240), 12, 0);

    const auto issue = findIssue(validate(), "V11");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->message, "総ノーツ数: 2（total の目安: 2）");
}

TEST_F(ValidatorTest, VAL012_WarnsWhenModeDefinitionIsMissing) {
    const auto issues = validateProject(project, ValidationContext{nullptr, &fileSystem, {}});

    const auto issue = findIssue(issues, "V00");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Warning);
}

TEST_F(ValidatorTest, VAL013_WarnsAboutPlacementsDesyncedByTempoChange) {
    const CutPointId head = cutPointAt("a.wav", 960);
    project.markTempoAffected(project.cutPoint(head).placementId);

    const auto issue = findIssue(validate(), "V12");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Warning);
    EXPECT_NE(issue->message.find("y=960"), std::string::npos);
}

TEST_F(ValidatorTest, VAL014_WarnsWhenBannerIsNot15To4) {
    const auto wide = validateProject(project, ValidationContext{&mode, &fileSystem, ImageSize{300, 80}});
    const auto square = validateProject(project, ValidationContext{&mode, &fileSystem, ImageSize{256, 256}});

    EXPECT_FALSE(findIssue(wide, "V10").has_value());
    const auto issue = findIssue(square, "V10");
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->severity, Severity::Warning);
}

}  // namespace
}  // namespace ibs::core
