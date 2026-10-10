#include "ibs/app/SlicingService.hpp"

#include <exception>

#include "ibs/core/asset/AssetPath.hpp"
#include "ibs/infra/audio/AudioFileDecoder.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

core::AudioBuffer LocalAudioSource::load(const std::filesystem::path& path) { return infra::decodeAudioFile(path); }

SlicingService::SlicingService(ChartStorage& storage, AudioSource& audio) : storage_(storage), audio_(audio) {}

SliceOutcome SlicingService::sliceBuffer(core::Project& project, core::PlacementId placementId,
                                         const core::AudioBuffer& audio, const SliceSettings& settings) {
    SliceOutcome outcome;
    const auto onsets = core::detectOnsets(core::mixToMono(audio), audio.sampleRate, settings.onset);
    outcome.detectedOnsets = onsets.size();
    outcome.plan = core::planCuts(onsets, audio.sampleRate, project.placement(placementId).startPulse, project.timing,
                                  settings.quantize);
    outcome.applied = core::applyCutPlan(project, placementId, outcome.plan);
    outcome.issues = core::attackClipIssues(project, placementId, outcome.plan);
    outcome.succeeded = true;
    return outcome;
}

SliceOutcome SlicingService::slicePlacement(core::Project& project, core::PlacementId placementId,
                                            const std::filesystem::path& chartDirectory,
                                            const SliceSettings& settings) const {
    try {
        const auto& assetName = project.asset(project.placement(placementId).assetId).name;
        const auto fileSystem = storage_.fileSystemAt(chartDirectory);
        const auto resolved = core::resolveSoundFile(assetName, *fileSystem);
        if (!resolved) {
            SliceOutcome outcome;
            outcome.error = "音素材 '" + assetName + "' が見つかりません";
            return outcome;
        }
        return sliceBuffer(project, placementId, audio_.load(chartDirectory / infra::pathFromUtf8(*resolved)), settings);
    } catch (const std::exception& error) {
        SliceOutcome outcome;
        outcome.error = error.what();
        return outcome;
    }
}

}  // namespace ibs::app
