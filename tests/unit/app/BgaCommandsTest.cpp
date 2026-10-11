#include <gtest/gtest.h>

#include "ibs/app/BgaCommands.hpp"

namespace ibs::app {
namespace {

TEST(BgaCommands, BGC001_EachChangeIsOneUndoStepAndNoOpsAddNone) {
    EditorDocument document;

    const auto id = addBgaImage(document, "movie.webm");
    EXPECT_EQ(addBgaImage(document, "movie.webm"), id);
    placeBgaEvent(document, core::BgaTrack::Bga, 0, id);
    placeBgaEvent(document, core::BgaTrack::Bga, 0, id);
    moveBgaEvent(document, core::BgaTrack::Bga, 0, 0);
    EXPECT_EQ(document.undoLabel(), "BGA の配置");

    moveBgaEvent(document, core::BgaTrack::Bga, 0, 960);
    EXPECT_EQ(document.project().bga.bgaEvents.front().y, 960);
    removeBgaImage(document, id);
    EXPECT_TRUE(document.project().bga.bgaEvents.empty());
    document.undo();
    EXPECT_EQ(document.project().bga.bgaEvents.size(), 1u);
}

TEST(BgaCommands, BGC002_SongInfoChangesOnlyWhenDifferent) {
    EditorDocument document;
    core::SongInfo info = document.project().info;

    updateSongInfo(document, info);
    EXPECT_FALSE(document.canUndo());
    info.bannerImage = "banner.png";
    updateSongInfo(document, info);
    EXPECT_EQ(document.project().info.bannerImage, "banner.png");
    EXPECT_EQ(document.undoLabel(), "曲の情報");
}

}  // namespace
}  // namespace ibs::app
