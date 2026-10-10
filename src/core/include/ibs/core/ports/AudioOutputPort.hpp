#pragma once

#include "ibs/core/audio/AudioBuffer.hpp"

namespace ibs::core {

class AudioOutputPort {
public:
    virtual ~AudioOutputPort() = default;

    [[nodiscard]] virtual int sampleRate() const = 0;
    virtual bool start(AudioBuffer stereo) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isPlaying() const = 0;
    [[nodiscard]] virtual SampleIndex playedFrames() const = 0;
};

}  // namespace ibs::core
