#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/LocalFileSystem.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::infra {
namespace {

class FileIoTest : public ::testing::Test {
protected:
    std::filesystem::path root;

    void SetUp() override {
        std::random_device device;
        root = std::filesystem::temp_directory_path() /
               pathFromUtf8("ibs_test_日本語_" + std::to_string(device()));
        std::filesystem::create_directories(root);
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    static core::bmson::Document sampleDocument() {
        core::bmson::Document document;
        document.info.title = "譜面";
        document.timing.initialBpm = 150.0;
        document.soundChannels = {{"ドラム/キック.wav", {{0, 0, 0, false}, {1, 240, 0, true}}}};
        return document;
    }
};

TEST_F(FileIoTest, FIO001_SavesAndLoadsWithJapanesePath) {
    const auto path = root / pathFromUtf8("曲[ANOTHER].bmson");
    const auto original = sampleDocument();

    saveBmsonFile(path, original);
    const auto loaded = loadBmsonFile(path).document;

    EXPECT_EQ(loaded.info, original.info);
    EXPECT_EQ(loaded.soundChannels, original.soundChannels);
}

TEST_F(FileIoTest, FIO002_OverwritesWithoutLeavingTemporaryFile) {
    const auto path = root / "chart.bmson";
    writeTextFileAtomically(path, "old");

    saveBmsonFile(path, sampleDocument());

    EXPECT_NE(readTextFile(path), "old");
    EXPECT_FALSE(std::filesystem::exists(root / "chart.bmson.tmp"));
}

TEST_F(FileIoTest, FIO003_LocalFileSystemFindsJapaneseFilesButNotDirectories) {
    std::filesystem::create_directories(root / pathFromUtf8("ドラム"));
    std::ofstream(root / pathFromUtf8("ドラム/キック.ogg")).put('x');
    const LocalFileSystem fileSystem(root);

    EXPECT_TRUE(fileSystem.exists("ドラム/キック.ogg"));
    EXPECT_FALSE(fileSystem.exists("ドラム"));
    EXPECT_FALSE(fileSystem.exists("ドラム/スネア.ogg"));
}

TEST_F(FileIoTest, FIO004_ReadingMissingFileThrows) {
    EXPECT_THROW(static_cast<void>(readTextFile(root / "missing.bmson")), std::runtime_error);
}

}  // namespace
}  // namespace ibs::infra
