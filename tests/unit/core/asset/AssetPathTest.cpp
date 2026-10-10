#include <string>

#include <gtest/gtest.h>

#include "../../support/FakeFileSystem.hpp"
#include "ibs/core/asset/AssetPath.hpp"

namespace ibs::core {
namespace {

using ibs::testing::FakeFileSystem;

TEST(AssetPath, AST001_RejectsAbsolutePaths) {
    for (const std::string path : {"C:\\a.wav", "/etc/a.wav", "\\a.wav"}) {
        const auto reason = findUnsafePathReason(path);
        ASSERT_TRUE(reason.has_value()) << path;
        EXPECT_EQ(*reason, "絶対パスです") << path;
    }
}

TEST(AssetPath, AST002_RejectsParentDirectoryReferences) {
    for (const std::string path : {"../a.wav", "sub/../../a.wav", "sub\\..\\a.wav"}) {
        const auto reason = findUnsafePathReason(path);
        ASSERT_TRUE(reason.has_value()) << path;
        EXPECT_EQ(*reason, "親フォルダ (..) を参照しています") << path;
    }
}

TEST(AssetPath, AST003_RejectsNulCharacterAndEmptyName) {
    EXPECT_TRUE(findUnsafePathReason(std::string("a\0.wav", 6)).has_value());
    EXPECT_TRUE(findUnsafePathReason("").has_value());
}

TEST(AssetPath, AST004_AcceptsRelativePathsIncludingJapanese) {
    for (const std::string path : {"drums/kick.wav", "intro\\drum", "ドラム/キック.wav"}) {
        EXPECT_FALSE(findUnsafePathReason(path).has_value()) << path;
    }
}

TEST(AssetPath, AST005_FindsFileWithDifferentExtension) {
    const FakeFileSystem fileSystem({"kick.ogg"});

    EXPECT_EQ(resolveSoundFile("kick.wav", fileSystem), "kick.ogg");
}

TEST(AssetPath, AST006_FindsFileForNameWithoutExtension) {
    const FakeFileSystem fileSystem({"piano.m4a"});

    EXPECT_EQ(resolveSoundFile("piano", fileSystem), "piano.m4a");
}

TEST(AssetPath, AST007_NormalizesBackslashSeparators) {
    const FakeFileSystem fileSystem({"intro/drum.wav"});

    EXPECT_EQ(resolveSoundFile("intro\\drum.wav", fileSystem), "intro/drum.wav");
}

TEST(AssetPath, AST008_ReturnsNothingWhenNoCandidateExists) {
    const FakeFileSystem fileSystem({"snare.wav"});

    EXPECT_FALSE(resolveSoundFile("kick.wav", fileSystem).has_value());
}

}  // namespace
}  // namespace ibs::core
