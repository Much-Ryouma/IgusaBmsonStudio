#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "../support/Beat7k.hpp"
#include "../support/FakeFileSystem.hpp"
#include "ibs/app/ChartService.hpp"

namespace ibs::app {
namespace {

class FakeChartStorage final : public ChartStorage {
public:
    std::optional<infra::BmsonParseResult> nextLoad;
    std::string loadError;
    std::set<std::string> existingFiles;
    std::optional<core::bmson::Document> savedDocument;

    infra::BmsonParseResult load(const std::filesystem::path&) override {
        if (!loadError.empty()) {
            throw infra::BmsonFormatError(loadError);
        }
        return *nextLoad;
    }

    void save(const std::filesystem::path&, const core::bmson::Document& document) override {
        savedDocument = document;
    }

    std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path&) override {
        return std::make_unique<ibs::testing::FakeFileSystem>(existingFiles);
    }
};

core::KeyModeCatalog catalogWithBeat7k() {
    core::KeyModeCatalog catalog;
    catalog.add(ibs::testing::beat7k());
    return catalog;
}

core::bmson::Document documentUsing(const std::string& asset) {
    core::bmson::Document document;
    document.timing.initialBpm = 120.0;
    document.soundChannels = {{asset, {{0, 0, 0, false}, {0, 240, 0, true}, {3, 240, 0, true}}}};
    return document;
}

bool containsCode(const std::vector<core::Issue>& issues, const std::string& code) {
    return std::any_of(issues.begin(), issues.end(), [&](const core::Issue& issue) { return issue.code == code; });
}

TEST(ChartService, APP001_OpenReturnsProjectWithAllNotices) {
    FakeChartStorage storage;
    storage.nextLoad = infra::BmsonParseResult{documentUsing("kick.wav"),
                                               {{core::Severity::Warning, "JSN-UNKNOWN-FIELD", "x"}}};
    const ChartService service(storage, catalogWithBeat7k());

    const auto outcome = service.open("chart.bmson");

    ASSERT_TRUE(outcome.chart.has_value());
    EXPECT_EQ(outcome.chart->project.assets().size(), 1u);
    EXPECT_TRUE(containsCode(outcome.chart->notices, "JSN-UNKNOWN-FIELD"));
    EXPECT_TRUE(containsCode(outcome.chart->notices, "IMP-BGM-SHADOWED"));
}

TEST(ChartService, APP002_OpenReportsFormatErrorWithFileName) {
    FakeChartStorage storage;
    storage.loadError = "version がありません";
    const ChartService service(storage, catalogWithBeat7k());

    const auto outcome = service.open("broken.bmson");

    EXPECT_FALSE(outcome.chart.has_value());
    EXPECT_NE(outcome.error.find("broken.bmson"), std::string::npos);
}

TEST(ChartService, APP003_SaveRefusesProjectWithValidationErrors) {
    FakeChartStorage storage;
    storage.nextLoad = infra::BmsonParseResult{documentUsing("missing.wav"), {}};
    const ChartService service(storage, catalogWithBeat7k());
    const auto opened = service.open("chart.bmson");

    const auto outcome = service.save(opened.chart->project, "out.bmson");

    EXPECT_FALSE(outcome.saved);
    EXPECT_TRUE(containsCode(outcome.issues, "V04"));
    EXPECT_FALSE(storage.savedDocument.has_value());
}

TEST(ChartService, APP004_SaveWritesExportedDocument) {
    FakeChartStorage storage;
    storage.nextLoad = infra::BmsonParseResult{documentUsing("kick.wav"), {}};
    storage.existingFiles = {"kick.ogg"};
    const ChartService service(storage, catalogWithBeat7k());
    const auto opened = service.open("chart.bmson");

    const auto outcome = service.save(opened.chart->project, "out.bmson");

    EXPECT_TRUE(outcome.saved);
    ASSERT_TRUE(storage.savedDocument.has_value());
    ASSERT_EQ(storage.savedDocument->soundChannels.size(), 1u);
    EXPECT_EQ(storage.savedDocument->soundChannels[0].notes,
              (std::vector<core::bmson::Note>{{0, 0, 0, false}, {3, 240, 0, true}}));
}

TEST(ChartService, APP005_UnknownModeHasNoDefinition) {
    FakeChartStorage storage;
    const ChartService service(storage, catalogWithBeat7k());
    core::Project project;
    project.info.modeHint = "beat-14k";

    EXPECT_EQ(service.keyModeFor(project), nullptr);
}

}  // namespace
}  // namespace ibs::app
