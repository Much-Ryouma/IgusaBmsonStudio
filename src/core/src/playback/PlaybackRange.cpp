#include "ibs/core/playback/PlaybackRange.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ibs::core {

namespace {

constexpr int kOutputChannels = 2;

float sourceSample(const AudioBuffer& audio, std::size_t frame, int outputChannel) {
    const auto channels = static_cast<std::size_t>(audio.channels);
    const std::size_t channel = audio.channels == 1 ? 0 : static_cast<std::size_t>(std::min(outputChannel, audio.channels - 1));
    return audio.interleavedSamples[frame * channels + channel];
}

void mixSource(const Project& project, const TimingMap& timing, const MixSource& source, double fromSongSeconds,
               int outputSampleRate, AudioBuffer& output) {
    const AudioBuffer& audio = *source.audio;
    const std::size_t sourceFrames = audio.frameCount();
    if (sourceFrames < 2 || audio.sampleRate <= 0) {
        return;
    }
    const double placementSeconds = timing.secondsAt(project.placement(source.placementId).startPulse);
    const std::size_t outputFrames = output.frameCount();

    const double firstLocalSeconds = fromSongSeconds - placementSeconds;
    const auto firstFrame = static_cast<std::size_t>(std::max(0.0, std::ceil(-firstLocalSeconds * outputSampleRate)));
    for (std::size_t frame = firstFrame; frame < outputFrames; ++frame) {
        const double position = (firstLocalSeconds + static_cast<double>(frame) / outputSampleRate) * audio.sampleRate;
        if (position < 0.0) {
            continue;
        }
        const auto index = static_cast<std::size_t>(position);
        if (index + 1 >= sourceFrames) {
            break;
        }
        // Why not 最も近いサンプルを使う: 素材と出力のサンプルレートが違うとき、ざらついた音になって試聴の判断を誤らせるため線形補間する
        const auto fraction = static_cast<float>(position - static_cast<double>(index));
        for (int channel = 0; channel < kOutputChannels; ++channel) {
            const float a = sourceSample(audio, index, channel);
            const float b = sourceSample(audio, index + 1, channel);
            output.interleavedSamples[frame * kOutputChannels + static_cast<std::size_t>(channel)] +=
                source.gain * (a + (b - a) * fraction);
        }
    }
}

}  // namespace

SampleIndex sampleOffsetInPlacement(const Project& project, PlacementId placementId, Pulse songPulse, int sampleRate) {
    const TimingMap timing = project.timing.toTimingMap();
    const Pulse start = project.placement(placementId).startPulse;
    const double seconds = timing.secondsAt(songPulse) - timing.secondsAt(start);
    return std::max<SampleIndex>(0, std::llround(seconds * sampleRate));
}

SampleRange sliceRange(const Project& project, CutPointId cutPointId, int sampleRate, SampleIndex totalFrames) {
    const CutPoint& cut = project.cutPoint(cutPointId);
    const Placement& placement = project.placement(cut.placementId);
    const auto ordered = project.cutPointsOf(cut.placementId);
    const auto position = std::find(ordered.begin(), ordered.end(), cutPointId);

    SampleRange range;
    range.begin = sampleOffsetInPlacement(project, placement.id, placement.startPulse + cut.offsetPulses, sampleRate);
    range.end = totalFrames;
    if (position != ordered.end() && std::next(position) != ordered.end()) {
        const CutPoint& next = project.cutPoint(*std::next(position));
        range.end = sampleOffsetInPlacement(project, placement.id, placement.startPulse + next.offsetPulses, sampleRate);
    }
    range.begin = std::clamp<SampleIndex>(range.begin, 0, totalFrames);
    range.end = std::clamp<SampleIndex>(range.end, range.begin, totalFrames);
    return range;
}

Pulse placementEndPulse(const Project& project, PlacementId placementId, double durationSeconds) {
    const TimingMap timing = project.timing.toTimingMap();
    const Pulse start = project.placement(placementId).startPulse;
    return static_cast<Pulse>(std::ceil(timing.pulseAt(timing.secondsAt(start) + std::max(0.0, durationSeconds))));
}

AudioBuffer renderRange(const AudioBuffer& audio, SampleRange range, int outputSampleRate) {
    if (outputSampleRate <= 0) {
        throw std::invalid_argument("outputSampleRate must be positive");
    }
    AudioBuffer output;
    output.sampleRate = outputSampleRate;
    output.channels = kOutputChannels;
    const auto sourceFrames = static_cast<SampleIndex>(audio.frameCount());
    const SampleIndex begin = std::clamp<SampleIndex>(range.begin, 0, sourceFrames);
    const SampleIndex end = std::clamp<SampleIndex>(range.end, begin, sourceFrames);
    if (end <= begin || audio.sampleRate <= 0) {
        return output;
    }
    const double ratio = static_cast<double>(audio.sampleRate) / outputSampleRate;
    const auto frames = static_cast<std::size_t>(std::ceil(static_cast<double>(end - begin) / ratio));
    output.interleavedSamples.assign(frames * kOutputChannels, 0.0f);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double position = static_cast<double>(begin) + static_cast<double>(frame) * ratio;
        const auto index = static_cast<std::size_t>(position);
        const std::size_t nextIndex = std::min<std::size_t>(index + 1, static_cast<std::size_t>(end) - 1);
        const auto fraction = static_cast<float>(position - static_cast<double>(index));
        for (int channel = 0; channel < kOutputChannels; ++channel) {
            const float a = sourceSample(audio, index, channel);
            const float b = sourceSample(audio, nextIndex, channel);
            output.interleavedSamples[frame * kOutputChannels + static_cast<std::size_t>(channel)] = a + (b - a) * fraction;
        }
    }
    return output;
}

AudioBuffer renderMix(const Project& project, const std::vector<MixSource>& sources, double fromSongSeconds,
                      double toSongSeconds, int outputSampleRate) {
    if (outputSampleRate <= 0) {
        throw std::invalid_argument("outputSampleRate must be positive");
    }
    AudioBuffer output;
    output.sampleRate = outputSampleRate;
    output.channels = kOutputChannels;
    const auto frames = static_cast<std::size_t>(
        std::max(0.0, std::ceil((toSongSeconds - fromSongSeconds) * outputSampleRate)));
    output.interleavedSamples.assign(frames * kOutputChannels, 0.0f);

    // Why not bmson の切り直し規則どおりに再生する: 同じ素材を順に鳴らす限り、切断点は鳴り方を変えないため、
    // 配置ごとに素材を頭から重ねるだけで実際のプレーヤーと同じ音になる
    const TimingMap timing = project.timing.toTimingMap();
    for (const auto& source : sources) {
        if (source.audio != nullptr) {
            mixSource(project, timing, source, fromSongSeconds, outputSampleRate, output);
        }
    }
    return output;
}

}  // namespace ibs::core
