#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/mode/KeyModeJsonLoader.hpp"

namespace ibs::infra {
namespace {

const std::filesystem::path kModesDirectory = std::filesystem::path(IBS_SOURCE_DIR) / "modes";

TEST(KeyModeJsonLoader, KMJ001_LoadsBundledBeat7kDefinition) {
    const auto mode = parseKeyMode(readTextFile(kModesDirectory / "beat-7k.json"));

    EXPECT_EQ(mode.modeHint(), "beat-7k");
    EXPECT_EQ(mode.lanes().size(), 8u);
    EXPECT_EQ(mode.bgmColumnStartX(), 9);
    const auto scratch = std::find_if(mode.lanes().begin(), mode.lanes().end(),
                                      [](const core::LaneDefinition& lane) { return lane.x == 8; });
    ASSERT_NE(scratch, mode.lanes().end());
    EXPECT_EQ(scratch->kind, core::LaneKind::Scratch);
}

TEST(KeyModeJsonLoader, KMJ002_RejectsDefinitionWithoutModeHint) {
    EXPECT_THROW(static_cast<void>(parseKeyMode(R"({"lanes": [{"x": 1}], "bgm_column_start_x": 2})")),
                 std::invalid_argument);
}

TEST(KeyModeJsonLoader, KMJ003_RejectsUnknownLaneKind) {
    EXPECT_THROW(static_cast<void>(parseKeyMode(
                     R"({"mode_hint": "m", "lanes": [{"x": 1, "kind": "pedal"}], "bgm_column_start_x": 2})")),
                 std::invalid_argument);
}

TEST(KeyModeJsonLoader, KMJ004_LoadsCatalogFromModesDirectory) {
    const auto catalog = loadKeyModeCatalog(kModesDirectory);

    EXPECT_NE(catalog.find("beat-7k"), nullptr);
}

}  // namespace
}  // namespace ibs::infra
