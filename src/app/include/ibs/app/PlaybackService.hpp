#pragma once

#include <optional>
#include <set>
#include <string>

#include "ibs/app/AudioLibrary.hpp"
#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/ports/AudioOutputPort.hpp"

namespace ibs::app {

using AudioOutput = core::AudioOutputPort;

enum class PlaybackKind { Slice, Placement, Song };

struct PlaybackContext {
    PlaybackKind kind = PlaybackKind::Song;
    std::optional<core::PlacementId> placementId;
    std::optional<core::CutPointId> cutPointId;
    double songStartSeconds = 0.0;
    double sourceStartSeconds = 0.0;
};

struct MixSettings {
    std::set<core::AssetId> muted;
    std::set<core::AssetId> soloed;

    [[nodiscard]] bool isAudible(core::AssetId asset) const;
};

struct PlaybackOutcome {
    bool started = false;
    std::string error;
};

class PlaybackService {
public:
    PlaybackService(AudioLibrary& library, AudioOutput& output);

    PlaybackOutcome playSlice(const core::Project& project, core::CutPointId cutPointId);
    PlaybackOutcome playPlacement(const core::Project& project, core::PlacementId placementId);
    PlaybackOutcome playSong(const core::Project& project, core::Pulse fromPulse, const MixSettings& mix);
    void stop();

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] const std::optional<PlaybackContext>& context() const noexcept { return context_; }
    [[nodiscard]] std::optional<double> playheadSongSeconds() const;
    [[nodiscard]] std::optional<double> playheadSourceSeconds() const;

private:
    PlaybackOutcome start(core::AudioBuffer buffer, PlaybackContext context);

    AudioLibrary& library_;
    AudioOutput& output_;
    std::optional<PlaybackContext> context_;
};

}  // namespace ibs::app
