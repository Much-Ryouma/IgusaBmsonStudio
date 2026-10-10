#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/app/EditorDocument.hpp"

namespace ibs::app {
namespace {

core::AssetId addKick(EditorDocument& document) {
    return document.apply("素材の追加", [](core::Project& project) { return project.addAsset("kick.wav"); });
}

TEST(EditorDocument, EDT001_EditCanBeUndoneAndRedone) {
    EditorDocument document;

    const core::AssetId asset = addKick(document);
    EXPECT_EQ(document.project().assets().size(), 1u);
    EXPECT_EQ(document.undoLabel(), "素材の追加");

    document.undo();
    EXPECT_TRUE(document.project().assets().empty());
    EXPECT_TRUE(document.canRedo());

    document.redo();
    ASSERT_EQ(document.project().assets().size(), 1u);
    EXPECT_EQ(document.project().assets().front().id, asset);
}

TEST(EditorDocument, EDT002_FailedEditLeavesDocumentUntouched) {
    EditorDocument document;
    addKick(document);

    EXPECT_THROW(document.apply("重複", [](core::Project& project) { project.addAsset("kick.wav"); }),
                 std::invalid_argument);

    EXPECT_EQ(document.project().assets().size(), 1u);
    EXPECT_EQ(document.undoLabel(), "素材の追加");
}

TEST(EditorDocument, EDT003_NewEditAfterUndoDiscardsRedo) {
    EditorDocument document;
    addKick(document);
    document.undo();

    document.apply("別の素材", [](core::Project& project) { project.addAsset("snare.wav"); });

    EXPECT_FALSE(document.canRedo());
}

TEST(EditorDocument, EDT004_KeepsOnlyTheNewestUndoSteps) {
    EditorDocument document({}, std::nullopt, 2);
    for (const char* name : {"a.wav", "b.wav", "c.wav"}) {
        document.apply(name, [name](core::Project& project) { project.addAsset(name); });
    }

    document.undo();
    document.undo();
    document.undo();

    ASSERT_EQ(document.project().assets().size(), 1u);
    EXPECT_EQ(document.project().assets().front().name, "a.wav");
}

TEST(EditorDocument, EDT005_DirtyFlagFollowsTheSavedRevision) {
    EditorDocument document;
    EXPECT_FALSE(document.isDirty());
    addKick(document);
    EXPECT_TRUE(document.isDirty());

    document.markSaved("song/song.bmson");
    EXPECT_FALSE(document.isDirty());
    document.undo();
    EXPECT_TRUE(document.isDirty());
    document.redo();
    EXPECT_FALSE(document.isDirty());
}

TEST(EditorDocument, EDT006_NotifiesListenersUntilRemoved) {
    EditorDocument document;
    int calls = 0;
    const auto id = document.addListener([&calls] { ++calls; });

    addKick(document);
    document.undo();
    document.redo();
    document.removeListener(id);
    document.undo();

    EXPECT_EQ(calls, 3);
}

TEST(EditorDocument, EDT007_ReplaceStartsCleanHistoryAtTheNewPath) {
    EditorDocument document;
    addKick(document);

    document.replace(core::Project{}, std::filesystem::path("charts") / "song.bmson");

    EXPECT_FALSE(document.canUndo());
    EXPECT_FALSE(document.isDirty());
    EXPECT_EQ(document.chartDirectory(), std::filesystem::path("charts"));
    EXPECT_EQ(EditorDocument{}.chartDirectory(), std::filesystem::path{});
}

}  // namespace
}  // namespace ibs::app
