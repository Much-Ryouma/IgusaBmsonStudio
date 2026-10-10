#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "ibs/infra/bmson/BmsonJsonCodec.hpp"

namespace ibs::infra {
namespace {

std::string minimalBmson(const std::string& infoExtra = "", const std::string& topExtra = "") {
    return R"({"version": "1.0.0", "info": {"title": "t", "init_bpm": 120)" + infoExtra + R"(}, "sound_channels": [])" +
           topExtra + "}";
}

bool hasNotice(const BmsonParseResult& result, const std::string& code) {
    return std::any_of(result.notices.begin(), result.notices.end(),
                       [&](const core::Issue& issue) { return issue.code == code; });
}

TEST(BmsonJsonCodec, JSN001_AppliesSpecDefaults) {
    const auto document = parseBmson(minimalBmson()).document;

    EXPECT_EQ(document.info.modeHint, "beat-7k");
    EXPECT_DOUBLE_EQ(document.info.judgeRank, 100.0);
    EXPECT_DOUBLE_EQ(document.info.total, 100.0);
    EXPECT_EQ(document.timing.resolution, 240);
    EXPECT_FALSE(document.timing.barLines.has_value());
}

TEST(BmsonJsonCodec, JSN002_AcceptsUtf8Bom) {
    EXPECT_NO_THROW(static_cast<void>(parseBmson("\xEF\xBB\xBF" + minimalBmson())));
}

TEST(BmsonJsonCodec, JSN003_RejectsLegacyFileWithoutVersion) {
    EXPECT_THROW(static_cast<void>(parseBmson(R"({"info": {"init_bpm": 120}, "sound_channels": []})")),
                 BmsonFormatError);
}

TEST(BmsonJsonCodec, JSN004_RejectsUnsupportedVersion) {
    EXPECT_THROW(
        static_cast<void>(parseBmson(R"({"version": "0.21", "info": {"init_bpm": 120}, "sound_channels": []})")),
        BmsonFormatError);
}

TEST(BmsonJsonCodec, JSN005_RejectsMissingInitialBpm) {
    EXPECT_THROW(static_cast<void>(parseBmson(R"({"version": "1.0.0", "info": {"title": "t"}, "sound_channels": []})")),
                 BmsonFormatError);
}

TEST(BmsonJsonCodec, JSN006_NormalizesResolution) {
    EXPECT_EQ(parseBmson(minimalBmson(R"(, "resolution": 0)")).document.timing.resolution, 240);
    EXPECT_EQ(parseBmson(minimalBmson(R"(, "resolution": null)")).document.timing.resolution, 240);

    const auto negative = parseBmson(minimalBmson(R"(, "resolution": -96)"));
    EXPECT_EQ(negative.document.timing.resolution, 96);
    EXPECT_TRUE(hasNotice(negative, "JSN-NEGATIVE-RESOLUTION"));
}

TEST(BmsonJsonCodec, JSN007_DefaultsOmittedNoteFields) {
    const auto document = parseBmson(R"({"version": "1.0.0", "info": {"init_bpm": 120},
        "sound_channels": [{"name": "a.wav", "notes": [{"x": null, "y": 240}]}]})")
                              .document;

    const auto& note = document.soundChannels.at(0).notes.at(0);
    EXPECT_EQ(note.x, 0);
    EXPECT_EQ(note.l, 0);
    EXPECT_FALSE(note.c);
}

TEST(BmsonJsonCodec, JSN008_KeepsEmptyLinesDistinctFromMissingLines) {
    const auto document = parseBmson(minimalBmson("", R"(, "lines": [])")).document;

    ASSERT_TRUE(document.timing.barLines.has_value());
    EXPECT_TRUE(document.timing.barLines->empty());
}

TEST(BmsonJsonCodec, JSN009_ReportsUnknownFields) {
    const auto result = parseBmson(minimalBmson(R"(, "custom": 1)", R"(, "extension": {})"));

    EXPECT_EQ(std::count_if(result.notices.begin(), result.notices.end(),
                            [](const core::Issue& issue) { return issue.code == "JSN-UNKNOWN-FIELD"; }),
              2);
}

TEST(BmsonJsonCodec, JSN010_AcceptsOnlyIntegralPulses) {
    const std::string prefix = R"({"version": "1.0.0", "info": {"init_bpm": 120}, "sound_channels": [{"name": "a", "notes": [{"x": 1, "y": )";

    EXPECT_THROW(static_cast<void>(parseBmson(prefix + "240.5}]}]}")), BmsonFormatError);
    EXPECT_EQ(parseBmson(prefix + "240.0}]}]}").document.soundChannels[0].notes[0].y, 240);
}

TEST(BmsonJsonCodec, JSN011_SerializedDocumentParsesBackToSameContent) {
    core::bmson::Document original;
    original.info.title = "曲名";
    original.info.subartists = {"chart:someone"};
    original.info.level = 7;
    original.timing.initialBpm = 174.5;
    original.timing.resolution = 96;
    original.timing.barLines = std::vector<core::Pulse>{0, 384};
    original.timing.bpmEvents = {{384, 87.25}};
    original.timing.stopEvents = {{192, 48}};
    original.soundChannels = {{"drums/kick.wav", {{0, 0, 0, false}, {1, 96, 48, true}}}};
    original.bga.headers = {{1, "bga.mp4"}};
    original.bga.bgaEvents = {{0, 1}};
    original.bga.poorEvents = {{0, 1}};

    const auto parsed = parseBmson(serializeBmson(original)).document;

    EXPECT_EQ(parsed.version, original.version);
    EXPECT_EQ(parsed.info, original.info);
    EXPECT_EQ(parsed.timing, original.timing);
    EXPECT_EQ(parsed.soundChannels, original.soundChannels);
    EXPECT_EQ(parsed.bga, original.bga);
}

TEST(BmsonJsonCodec, JSN012_WritesIntegralNumbersWithoutFraction) {
    core::bmson::Document document;
    document.timing.initialBpm = 100.0;

    EXPECT_NE(serializeBmson(document).find(R"("init_bpm": 100,)"), std::string::npos);
}

TEST(BmsonJsonCodec, JSN013_WritesOneNotePerLine) {
    core::bmson::Document document;
    document.soundChannels = {{"a.wav", {{1, 240, 0, true}}}};

    EXPECT_NE(serializeBmson(document).find(R"({"x": 1, "y": 240, "l": 0, "c": true})"), std::string::npos);
}

TEST(BmsonJsonCodec, JSN014_KeepsJapaneseTextUnescaped) {
    core::bmson::Document document;
    document.info.title = "伊草";

    EXPECT_NE(serializeBmson(document).find("\"title\": \"伊草\""), std::string::npos);
}

}  // namespace
}  // namespace ibs::infra
