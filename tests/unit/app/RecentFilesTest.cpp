#include <gtest/gtest.h>

#include "ibs/app/RecentFiles.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {
namespace {

std::string native(const char* path) {
    return infra::pathToUtf8(std::filesystem::path(path).make_preferred());
}

TEST(RecentFiles, RCF001_NewestFirstWithoutDuplicates) {
    std::vector<std::string> recent;
    recent = withRecentFile(recent, "songs/a.bmson");
    recent = withRecentFile(recent, "songs/b.bmson");
    recent = withRecentFile(recent, "songs/./a.bmson");

    EXPECT_EQ(recent, (std::vector<std::string>{native("songs/a.bmson"), native("songs/b.bmson")}));
}

TEST(RecentFiles, RCF002_KeepsOnlyTheLimit) {
    std::vector<std::string> recent;
    for (int index = 0; index < 12; ++index) {
        recent = withRecentFile(recent, "song" + std::to_string(index) + ".bmson");
    }

    ASSERT_EQ(recent.size(), kRecentFileLimit);
    EXPECT_EQ(recent.front(), "song11.bmson");
    EXPECT_EQ(recent.back(), "song2.bmson");
}

TEST(RecentFiles, RCF003_RemovesAFileThatCouldNotBeOpened) {
    const std::vector<std::string> recent{native("a.bmson"), native("b.bmson")};

    EXPECT_EQ(withoutRecentFile(recent, "a.bmson"), std::vector<std::string>{native("b.bmson")});
}

}  // namespace
}  // namespace ibs::app
