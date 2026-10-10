#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ibs/core/bmson/BmsonEquivalence.hpp"
#include "ibs/core/bmson/BmsonExporter.hpp"
#include "ibs/core/bmson/BmsonImporter.hpp"
#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs {
namespace {

std::optional<std::filesystem::path> externalSamplesDirectory() {
#if defined(_WIN32)
    // Why not std::getenv: Windows では値が CP932 で返り、日本語を含むフォルダ名が壊れるため、ワイド文字版で読む
    wchar_t* buffer = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&buffer, &length, L"IBS_EXTERNAL_SAMPLES_DIR") != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::filesystem::path directory(buffer);
    std::free(buffer);
#else
    const char* value = std::getenv("IBS_EXTERNAL_SAMPLES_DIR");
    if (value == nullptr) {
        return std::nullopt;
    }
    std::filesystem::path directory = infra::pathFromUtf8(value);
#endif
    if (directory.empty()) {
        return std::nullopt;
    }
    return directory;
}

std::vector<std::filesystem::path> externalSampleCharts() {
    const auto directory = externalSamplesDirectory();
    if (!directory) {
        return {};
    }
    std::vector<std::filesystem::path> charts;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(*directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bmson") {
            charts.push_back(entry.path());
        }
    }
    return charts;
}

std::size_t countNotes(const core::bmson::Document& document) {
    std::size_t count = 0;
    for (const auto& channel : document.soundChannels) {
        count += core::bmson::normalizedForPlayback(channel).notes.size();
    }
    return count;
}

TEST(SampleRoundTrip, EXT001_EXT002_EveryExternalChartSurvivesRoundTrip) {
    const auto charts = externalSampleCharts();
    if (charts.empty()) {
        GTEST_SKIP() << "IBS_EXTERNAL_SAMPLES_DIR が未設定、または bmson がありません";
    }

    for (const auto& chart : charts) {
        SCOPED_TRACE(infra::pathToUtf8(chart));
        const auto original = infra::loadBmsonFile(chart).document;
        const auto exported = core::bmson::exportDocument(core::bmson::importDocument(original).project);
        const auto reparsed = infra::parseBmson(infra::serializeBmson(exported)).document;

        const auto differences = core::bmson::describePlaybackDifferences(original, reparsed);
        for (const auto& difference : differences) {
            ADD_FAILURE() << difference;
        }
        EXPECT_EQ(countNotes(original), countNotes(reparsed));
    }
}

}  // namespace
}  // namespace ibs
