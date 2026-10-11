#include <gtest/gtest.h>

#include "ibs/infra/sidecar/SidecarJsonCodec.hpp"

namespace ibs::infra {
namespace {

TEST(SidecarJsonCodec, SCJ001_RoundTripsEveryField) {
    core::SidecarData data;
    core::SidecarPlacement placement;
    placement.asset = "ベース.wav";
    placement.startPulse = 1920;
    placement.tempoAffected = true;
    placement.cuts = {{0, core::CutOrigin::Imported}, {240, core::CutOrigin::Auto}, {480, core::CutOrigin::Manual}};
    core::SliceSettings slice;
    slice.onset.function = core::OnsetFunction::HighFrequencyContent;
    slice.onset.sensitivity = 0.7;
    slice.quantize.divisionsPerWholeNote = 12;
    slice.quantize.windowMs = 15.0;
    placement.slice = slice;
    data.placements.push_back(placement);
    data.placements.push_back(core::SidecarPlacement{"lead.wav", 0, {}, false, std::nullopt});

    const auto parsed = parseSidecar(serializeSidecar(data));

    ASSERT_EQ(parsed.placements.size(), 2u);
    const auto& first = parsed.placements[0];
    EXPECT_EQ(first.asset, "ベース.wav");
    EXPECT_EQ(first.startPulse, 1920);
    EXPECT_TRUE(first.tempoAffected);
    ASSERT_EQ(first.cuts.size(), 3u);
    EXPECT_EQ(first.cuts[2].origin, core::CutOrigin::Manual);
    ASSERT_TRUE(first.slice.has_value());
    EXPECT_EQ(first.slice->onset.function, core::OnsetFunction::HighFrequencyContent);
    EXPECT_DOUBLE_EQ(first.slice->onset.sensitivity, 0.7);
    EXPECT_EQ(first.slice->quantize.divisionsPerWholeNote, 12);
    EXPECT_EQ(first.slice->quantize.windowMs, 15.0);
    EXPECT_FALSE(parsed.placements[1].slice.has_value());
}

TEST(SidecarJsonCodec, SCJ002_RejectsForeignOrNewerFiles) {
    EXPECT_THROW(static_cast<void>(parseSidecar("{\"version\": 1}")), SidecarFormatError);
    EXPECT_THROW(static_cast<void>(parseSidecar("{\"format\": \"igusa-bmson-studio-sidecar\", \"version\": 99}")), SidecarFormatError);
    EXPECT_THROW(static_cast<void>(parseSidecar("not json")), SidecarFormatError);
    EXPECT_THROW(static_cast<void>(parseSidecar(
                     "{\"format\": \"igusa-bmson-studio-sidecar\", \"version\": 1, \"placements\": "
                     "[{\"asset\": \"a.wav\", \"y\": 0, \"cuts\": [{\"offset\": 0, \"origin\": \"magic\"}]}]}")),
                 SidecarFormatError);
}

TEST(SidecarJsonCodec, SCJ003_SidecarSitsNextToTheBmsonWithItsName) {
    EXPECT_EQ(sidecarPathFor(std::filesystem::path("song") / "song[ANOTHER].bmson"),
              std::filesystem::path("song") / "song[ANOTHER].ibs.json");
    EXPECT_FALSE(loadSidecarFile(std::filesystem::path("no-such-folder") / "x.ibs.json").has_value());
}

}  // namespace
}  // namespace ibs::infra
