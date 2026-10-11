#include <gtest/gtest.h>

#include "ibs/app/PreviewService.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {
namespace {

class RecordingStorage final : public ChartStorage {
public:
    infra::BmsonParseResult load(const std::filesystem::path&) override { return {}; }
    void save(const std::filesystem::path& path, const core::bmson::Document&) override { savedPaths.push_back(path); }
    std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path&) override { return nullptr; }

    std::vector<std::filesystem::path> savedPaths;
};

class RecordingLauncher final : public ProcessLauncher {
public:
    bool launch(const LaunchRequest& request) override {
        requests.push_back(request);
        return succeed;
    }

    bool succeed = true;
    std::vector<LaunchRequest> requests;
};

TEST(PreviewService, PRV001_SplitsCommandLinesRespectingQuotes) {
    EXPECT_EQ(splitCommandLine(R"("C:\Program Files\Pulsus\Pulsus.exe" "{file}" -p  -m {measure})"),
              (std::vector<std::string>{R"(C:\Program Files\Pulsus\Pulsus.exe)", "{file}", "-p", "-m", "{measure}"}));
    EXPECT_EQ(splitCommandLine(R"(a "" b)"), (std::vector<std::string>{"a", "", "b"}));
}

TEST(PreviewService, PRV002_FillsPlaceholdersAfterSplitting) {
    PlayerSettings settings{R"(C:\Players\Pulsus.exe)", R"("{exe}" "{file}" -p -m {measure} --at={pulse}/{seconds})"};
    const auto file = std::filesystem::path("My Song") / "song [A].bmson";

    const auto request = buildLaunchRequest(settings, file, PreviewPosition{12, 11520, 24.0});

    EXPECT_EQ(request.program, R"(C:\Players\Pulsus.exe)");
    EXPECT_EQ(request.arguments,
              (std::vector<std::string>{infra::pathToUtf8(file), "-p", "-m", "12", "--at=11520/24.000"}));
    EXPECT_EQ(request.workingDirectory, std::filesystem::path("My Song"));
}

TEST(PreviewService, PRV003_MeasureComesFromTheBarLinesAndIsClamped) {
    core::Project project;
    project.timing.initialBpm = 120.0;
    project.timing.resolution = 240;
    project.timing.barLines = std::vector<core::Pulse>{0, 960, 1800};

    EXPECT_EQ(previewPositionAt(project, 1800).measure, 2);
    EXPECT_DOUBLE_EQ(previewPositionAt(project, 1800).seconds, 3.75);
    EXPECT_EQ(previewPositionAt(project, 840 * 2000).measure, 999);
    EXPECT_EQ(previewPositionAt(project, -5).pulse, 0);
}

TEST(PreviewService, PRV004_UnsavedChangesArePreviewedFromATemporaryFileNextToTheChart) {
    RecordingStorage storage;
    RecordingLauncher launcher;
    const ChartService charts(storage, {});
    PreviewService service(charts, launcher);
    EditorDocument document({}, std::filesystem::path("song") / "song.bmson");
    const PlayerSettings settings{"Pulsus.exe"};

    const auto clean = service.preview(document, 0, settings);
    document.apply("素材", [](core::Project& project) { project.addAsset("a.wav"); });
    const auto dirty = service.preview(document, 0, settings);

    EXPECT_TRUE(clean.launched);
    EXPECT_EQ(clean.file, std::filesystem::path("song") / "song.bmson");
    EXPECT_EQ(dirty.file, std::filesystem::path("song") / ".~preview_song.bmson");
    EXPECT_EQ(storage.savedPaths, std::vector<std::filesystem::path>{dirty.file});
    ASSERT_EQ(launcher.requests.size(), 2u);
    EXPECT_EQ(launcher.requests[1].arguments.front(), infra::pathToUtf8(dirty.file));
}

TEST(PreviewService, PRV005_ExplainsWhyItCannotLaunch) {
    RecordingStorage storage;
    RecordingLauncher launcher;
    const ChartService charts(storage, {});
    PreviewService service(charts, launcher);
    EditorDocument unsaved;
    EditorDocument saved({}, std::filesystem::path("song.bmson"));

    EXPECT_FALSE(service.preview(saved, 0, PlayerSettings{}).error.empty());
    EXPECT_FALSE(service.preview(unsaved, 0, PlayerSettings{"Pulsus.exe"}).error.empty());
    launcher.succeed = false;
    const auto failed = service.preview(saved, 0, PlayerSettings{"Pulsus.exe"});
    EXPECT_FALSE(failed.launched);
    EXPECT_NE(failed.error.find("Pulsus.exe"), std::string::npos);
}

}  // namespace
}  // namespace ibs::app
