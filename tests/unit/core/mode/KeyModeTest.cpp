#include <stdexcept>

#include <gtest/gtest.h>

#include "../../support/Beat7k.hpp"
#include "ibs/core/mode/KeyMode.hpp"

namespace ibs::core {
namespace {

TEST(KeyMode, KEY001_OnlyDefinedLanesArePlayable) {
    const KeyMode mode = ibs::testing::beat7k();

    EXPECT_TRUE(mode.isPlayable(1));
    EXPECT_TRUE(mode.isPlayable(8));
    EXPECT_FALSE(mode.isPlayable(0));
    EXPECT_FALSE(mode.isPlayable(9));
}

TEST(KeyMode, KEY002_RejectsDuplicatedLaneX) {
    EXPECT_THROW(KeyMode("dup", "", {{1, "1", LaneKind::White, 1.0}, {1, "1", LaneKind::White, 1.0}}, 9),
                 std::invalid_argument);
}

TEST(KeyMode, KEY003_RejectsBgmColumnStartInsideLanes) {
    EXPECT_THROW(KeyMode("bad", "", {{1, "1", LaneKind::White, 1.0}, {8, "SC", LaneKind::Scratch, 2.0}}, 8),
                 std::invalid_argument);
}

TEST(KeyMode, KEY004_CatalogRejectsDuplicatedModeHint) {
    KeyModeCatalog catalog;
    catalog.add(ibs::testing::beat7k());

    EXPECT_THROW(catalog.add(ibs::testing::beat7k()), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
