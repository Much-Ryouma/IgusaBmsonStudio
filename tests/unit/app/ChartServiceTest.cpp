#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
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

    std::optional<core::SidecarData> loadSidecar(const std::filesystem::path&) override {
        if (sidecarLoadFails) {
            throw std::runtime_error("broken sidecar");
        }
        return storedSidecar;
    }

    void saveSidecar(const std::filesystem::path& path, const core::SidecarData& data) override {
        if (sidecarSaveFails) {
            throw std::runtime_error("read-only");
        }
        sidecarPath = path;
        storedSidecar = data;
    }

    std::optional<core::ImageSize> imageSize(const std::filesystem::path& path) override {
        imageQueries.push_back(path);
        return bannerSize;
    }

    std::optional<core::SidecarData> storedSidecar;
    std::filesystem::path sidecarPath;
    bool sidecarLoadFails = false;
    bool sidecarSaveFails = false;
    std::optional<core::ImageSize> bannerSize;
    std::vector<std::filesystem::path> imageQueries;
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

TEST(ChartService, APP006_SaveWritesSidecarAndOpenRestoresIt) {
    FakeChartStorage storage;
    storage.nextLoad = infra::BmsonParseResult{documentUsing("kick.wav"), {}};
    storage.existingFiles = {"kick.wav"};
    const ChartService service(storage, catalogWithBeat7k());
    auto opened = service.open("chart.bmson");
    auto& project = opened.chart->project;
    const auto placement = project.placements().front().id;
    project.setCutOrigin(project.cutPointsOf(placement)[1], core::CutOrigin::Manual);
    core::SliceSettings settings;
    settings.onset.sensitivity = 0.8;

    ASSERT_TRUE(service.save(project, "chart.bmson", {{placement, settings}}).saved);
    EXPECT_EQ(storage.sidecarPath, std::filesystem::path("chart.bmson"));
    const auto reopened = service.open("chart.bmson");

    const auto& restored = reopened.chart->project;
    const auto restoredPlacement = restored.placements().front().id;
    EXPECT_EQ(restored.cutPoint(restored.cutPointsOf(restoredPlacement)[1]).origin, core::CutOrigin::Manual);
    EXPECT_DOUBLE_EQ(reopened.chart->sliceSettings.at(restoredPlacement).onset.sensitivity, 0.8);
}

TEST(ChartService, APP007_SidecarProblemsNeverBlockTheBmson) {
    FakeChartStorage storage;
    storage.nextLoad = infra::BmsonParseResult{documentUsing("kick.wav"), {}};
    storage.existingFiles = {"kick.wav"};
    storage.sidecarLoadFails = true;
    storage.sidecarSaveFails = true;
    const ChartService service(storage, catalogWithBeat7k());

    const auto opened = service.open("chart.bmson");
    ASSERT_TRUE(opened.chart.has_value());
    EXPECT_TRUE(containsCode(opened.chart->notices, "SIDECAR-UNREADABLE"));
    const auto saved = service.save(opened.chart->project, "chart.bmson");
    EXPECT_TRUE(saved.saved);
    EXPECT_TRUE(containsCode(saved.issues, "SIDECAR-UNWRITABLE"));
}

TEST(ChartService, APP008_ChecksTheBannerImageSize) {
    FakeChartStorage storage;
    storage.existingFiles = {"banner.png"};
    storage.bannerSize = core::ImageSize{256, 256};
    const ChartService service(storage, catalogWithBeat7k());
    core::Project project;
    project.info.bannerImage = "banner.png";

    const auto issues = service.validate(project, "song");

    EXPECT_TRUE(containsCode(issues, "V10"));
    ASSERT_EQ(storage.imageQueries.size(), 1u);
    EXPECT_EQ(storage.imageQueries.front(), std::filesystem::path("song") / "banner.png");
}

TEST(ChartService, APP009_ExportCopyWritesEvenWithValidationErrors) {
    FakeChartStorage storage;
    storage.nextLoad = infra::BmsonParseResult{documentUsing("missing.wav"), {}};
    const ChartService service(storage, catalogWithBeat7k());
    const auto opened = service.open("chart.bmson");

    const auto outcome = service.exportCopy(opened.chart->project, ".~preview_chart.bmson");

    EXPECT_TRUE(outcome.saved);
    EXPECT_TRUE(containsCode(outcome.issues, "V04"));
    EXPECT_TRUE(storage.savedDocument.has_value());
    EXPECT_FALSE(storage.storedSidecar.has_value());
}

}  // namespace
}  // namespace ibs::app
