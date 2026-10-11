#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ibs/app/ChartService.hpp"
#include "ibs/app/PreviewService.hpp"
#include "ibs/app/ProjectAudio.hpp"
#include "ibs/app/SlicingService.hpp"
#include "ibs/core/playback/BmsonRenderer.hpp"

namespace ibs::app {

struct NullTestOptions {
    double thresholdDb = -30.0;
    std::size_t worstCount = 8;
    int internalSampleRate = 44100;
    core::RenderComparisonOptions comparison;
};

struct NullTestSection {
    double startSeconds = 0.0;
    double residualDb = 0.0;
    int bar = 0;
    core::Pulse pulse = 0;
};

struct NullTestReport {
    bool completed = false;
    std::string error;
    double offsetMs = 0.0;
    double gain = 1.0;
    double residualDb = 0.0;
    bool passed = false;
    std::vector<NullTestSection> worst;
    std::vector<std::string> missingAssets;
};

class ProcessRunner {
public:
    virtual ~ProcessRunner() = default;

    virtual int run(const LaunchRequest& request) = 0;
};

class LocalProcessRunner final : public ProcessRunner {
public:
    int run(const LaunchRequest& request) override;
};

class NullTestService {
public:
    NullTestService(const ChartService& charts, ChartStorage& storage, AudioSource& audio);

    [[nodiscard]] NullTestReport compareWithInternalPlayer(const core::Project& project,
                                                           const std::filesystem::path& chartDirectory,
                                                           const NullTestOptions& options = {}) const;
    [[nodiscard]] NullTestReport compareWithRendering(const core::Project& project,
                                                      const std::filesystem::path& chartDirectory,
                                                      const core::AudioBuffer& rendered,
                                                      const NullTestOptions& options = {}) const;
    [[nodiscard]] NullTestReport compareWithPlayer(const core::Project& project, const std::filesystem::path& chartPath,
                                                   const PlayerSettings& player, const std::string& renderTemplate,
                                                   ProcessRunner& runner, const NullTestOptions& options = {}) const;

private:
    [[nodiscard]] NullTestReport summarize(const core::Project& project, const core::AudioBuffer& reference,
                                           const core::AudioBuffer& candidate, const ProjectAudio& assets,
                                           const NullTestOptions& options) const;
    [[nodiscard]] core::AudioBuffer referenceMix(const core::Project& project, const ProjectAudio& assets, int sampleRate) const;

    const ChartService& charts_;
    ChartStorage& storage_;
    AudioSource& audio_;
};

}  // namespace ibs::app
