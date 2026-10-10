#include "ibs/app/PlaybackService.hpp"

#include <algorithm>
#include <exception>
#include <vector>

#include "ibs/core/playback/PlaybackRange.hpp"

namespace ibs::app {

bool MixSettings::isAudible(core::AssetId asset) const {
    if (!soloed.empty()) {
        return soloed.contains(asset);
    }
    return !muted.contains(asset);
}

PlaybackService::PlaybackService(AudioLibrary& library, AudioOutput& output) : library_(library), output_(output) {}

PlaybackOutcome PlaybackService::start(core::AudioBuffer buffer, PlaybackContext context) {
    output_.stop();
    context_.reset();
    if (buffer.frameCount() == 0) {
        return PlaybackOutcome{false, "再生する音がありません"};
    }
    if (!output_.start(std::move(buffer))) {
        return PlaybackOutcome{false, "オーディオ出力を開始できません"};
    }
    context_ = context;
    return PlaybackOutcome{true, {}};
}

PlaybackOutcome PlaybackService::playSlice(const core::Project& project, core::CutPointId cutPointId) {
    try {
        const core::CutPoint& cut = project.cutPoint(cutPointId);
        const core::Placement& placement = project.placement(cut.placementId);
        const auto lookup = library_.get(project.asset(placement.assetId).name);
        if (!lookup.audio) {
            return PlaybackOutcome{false, lookup.error};
        }
        const auto& audio = lookup.audio->audio;
        const auto range = core::sliceRange(project, cutPointId, audio.sampleRate,
                                            static_cast<core::SampleIndex>(audio.frameCount()));
        const auto timing = project.timing.toTimingMap();
        PlaybackContext context;
        context.kind = PlaybackKind::Slice;
        context.placementId = placement.id;
        context.cutPointId = cutPointId;
        context.songStartSeconds = timing.secondsAt(project.absolutePulseOf(cut));
        context.sourceStartSeconds = static_cast<double>(range.begin) / audio.sampleRate;
        return start(core::renderRange(audio, range, output_.sampleRate()), context);
    } catch (const std::exception& error) {
        return PlaybackOutcome{false, error.what()};
    }
}

PlaybackOutcome PlaybackService::playPlacement(const core::Project& project, core::PlacementId placementId) {
    try {
        const core::Placement& placement = project.placement(placementId);
        const auto lookup = library_.get(project.asset(placement.assetId).name);
        if (!lookup.audio) {
            return PlaybackOutcome{false, lookup.error};
        }
        const auto& audio = lookup.audio->audio;
        PlaybackContext context;
        context.kind = PlaybackKind::Placement;
        context.placementId = placementId;
        context.songStartSeconds = project.timing.toTimingMap().secondsAt(placement.startPulse);
        const core::SampleRange whole{0, static_cast<core::SampleIndex>(audio.frameCount())};
        return start(core::renderRange(audio, whole, output_.sampleRate()), context);
    } catch (const std::exception& error) {
        return PlaybackOutcome{false, error.what()};
    }
}

PlaybackOutcome PlaybackService::playSong(const core::Project& project, core::Pulse fromPulse, const MixSettings& mix) {
    try {
        const auto timing = project.timing.toTimingMap();
        const double fromSeconds = timing.secondsAt(std::max<core::Pulse>(0, fromPulse));
        double endSeconds = fromSeconds;
        std::vector<std::shared_ptr<const LoadedAudio>> keepAlive;
        std::vector<core::MixSource> sources;
        for (const auto& placement : project.placements()) {
            if (!mix.isAudible(placement.assetId)) {
                continue;
            }
            const auto lookup = library_.get(project.asset(placement.assetId).name);
            if (!lookup.audio) {
                continue;
            }
            keepAlive.push_back(lookup.audio);
            sources.push_back(core::MixSource{placement.id, &lookup.audio->audio, 1.0f});
            endSeconds = std::max(endSeconds, timing.secondsAt(placement.startPulse) + lookup.audio->audio.durationSeconds());
        }
        PlaybackContext context;
        context.kind = PlaybackKind::Song;
        context.songStartSeconds = fromSeconds;
        return start(core::renderMix(project, sources, fromSeconds, endSeconds, output_.sampleRate()), context);
    } catch (const std::exception& error) {
        return PlaybackOutcome{false, error.what()};
    }
}

void PlaybackService::stop() {
    output_.stop();
    context_.reset();
}

bool PlaybackService::isPlaying() const { return context_.has_value() && output_.isPlaying(); }

std::optional<double> PlaybackService::playheadSongSeconds() const {
    if (!isPlaying()) {
        return std::nullopt;
    }
    return context_->songStartSeconds + static_cast<double>(output_.playedFrames()) / output_.sampleRate();
}

std::optional<double> PlaybackService::playheadSourceSeconds() const {
    if (!isPlaying() || context_->kind == PlaybackKind::Song) {
        return std::nullopt;
    }
    return context_->sourceStartSeconds + static_cast<double>(output_.playedFrames()) / output_.sampleRate();
}

}  // namespace ibs::app
