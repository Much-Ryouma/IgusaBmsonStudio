#include <gtest/gtest.h>

#include "../support/FakeAppPorts.hpp"
#include "ibs/app/AssetImportService.hpp"

namespace ibs::app {
namespace {

using ibs::testing::FakeAssetFileStore;

const std::filesystem::path kChart = std::filesystem::path("work") / "song";

EditorDocument documentAtChart() { return EditorDocument({}, kChart / "song.bmson"); }

TEST(AssetImportService, IMP001_NamesFilesInsideTheChartFolderRelatively) {
    EXPECT_EQ(assetNameInside(kChart, kChart / "stems" / "kick.wav"), "stems/kick.wav");
    EXPECT_EQ(assetNameInside(kChart, kChart / "." / "kick.wav"), "kick.wav");
    EXPECT_FALSE(assetNameInside(kChart, std::filesystem::path("work") / "other" / "kick.wav").has_value());
}

TEST(AssetImportService, IMP002_ImportsInsideFileOnceWithoutCopying) {
    FakeAssetFileStore files;
    AssetImportService service(files);
    auto document = documentAtChart();

    const auto first = service.import(document, kChart / "kick.wav", OutsideFilePolicy::Reject);
    const auto second = service.import(document, kChart / "kick.wav", OutsideFilePolicy::Reject);

    ASSERT_TRUE(first.assetId.has_value()) << first.error;
    EXPECT_EQ(first.assetName, "kick.wav");
    EXPECT_FALSE(first.copied);
    EXPECT_TRUE(second.alreadyImported);
    EXPECT_EQ(second.assetId, first.assetId);
    EXPECT_EQ(document.project().assets().size(), 1u);
    EXPECT_TRUE(files.copies.empty());
}

TEST(AssetImportService, IMP003_CopiesOutsideFileIntoTheChartFolderOnlyWhenAllowed) {
    FakeAssetFileStore files;
    AssetImportService service(files);
    auto document = documentAtChart();
    const auto outside = std::filesystem::path("downloads") / "bass.wav";

    const auto rejected = service.import(document, outside, OutsideFilePolicy::Reject);
    const auto copied = service.import(document, outside, OutsideFilePolicy::Copy);

    EXPECT_FALSE(rejected.assetId.has_value());
    EXPECT_FALSE(rejected.error.empty());
    ASSERT_TRUE(copied.assetId.has_value()) << copied.error;
    EXPECT_TRUE(copied.copied);
    EXPECT_EQ(copied.assetName, "bass.wav");
    ASSERT_EQ(files.copies.size(), 1u);
    EXPECT_EQ(files.copies.front().second, kChart / "bass.wav");
}

TEST(AssetImportService, IMP004_NeverOverwritesAnUnrelatedFileInTheChartFolder) {
    FakeAssetFileStore files;
    files.files.insert(kChart / "bass.wav");
    AssetImportService service(files);
    auto document = documentAtChart();

    const auto outcome = service.import(document, std::filesystem::path("downloads") / "bass.wav", OutsideFilePolicy::Copy);

    EXPECT_FALSE(outcome.assetId.has_value());
    EXPECT_NE(outcome.error.find("bass.wav"), std::string::npos);
    EXPECT_TRUE(files.copies.empty());
}

TEST(AssetImportService, IMP005_NeedsAChartLocationFirst) {
    FakeAssetFileStore files;
    AssetImportService service(files);
    EditorDocument unsaved;

    const auto outcome = service.import(unsaved, "kick.wav", OutsideFilePolicy::Copy);

    EXPECT_FALSE(outcome.assetId.has_value());
    EXPECT_FALSE(outcome.error.empty());
}

TEST(AssetImportService, IMP006_ImportsBgaFilesLikeAudioButIntoTheBgaList) {
    FakeAssetFileStore files;
    AssetImportService service(files);
    auto document = documentAtChart();
    EditorDocument unsaved;

    const auto inside = service.importBga(document, kChart / "movie" / "bg.webm", OutsideFilePolicy::Reject);
    const auto again = service.importBga(document, kChart / "movie" / "bg.webm", OutsideFilePolicy::Reject);
    const auto rejected = service.importBga(document, std::filesystem::path("work") / "logo.png", OutsideFilePolicy::Reject);
    const auto copied = service.importBga(document, std::filesystem::path("work") / "logo.png", OutsideFilePolicy::Copy);
    const auto notImage = service.importBga(document, kChart / "kick.wav", OutsideFilePolicy::Copy);
    const auto noFolder = service.importBga(unsaved, kChart / "bg.png", OutsideFilePolicy::Copy);

    ASSERT_TRUE(inside.imageId.has_value()) << inside.error;
    EXPECT_EQ(inside.name, "movie/bg.webm");
    EXPECT_TRUE(again.alreadyImported);
    EXPECT_EQ(again.imageId, inside.imageId);
    EXPECT_FALSE(rejected.imageId.has_value());
    EXPECT_FALSE(rejected.error.empty());
    ASSERT_TRUE(copied.imageId.has_value()) << copied.error;
    EXPECT_TRUE(copied.copied);
    EXPECT_EQ(copied.name, "logo.png");
    ASSERT_EQ(files.copies.size(), 1u);
    EXPECT_EQ(files.copies.front().second, kChart / "logo.png");
    EXPECT_FALSE(notImage.imageId.has_value());
    EXPECT_FALSE(noFolder.imageId.has_value());
    EXPECT_EQ(document.project().bga.headers.size(), 2u);
    EXPECT_TRUE(isBgaFile("A.PNG"));
    EXPECT_FALSE(isBgaFile("song.bmson"));
}

}  // namespace
}  // namespace ibs::app
