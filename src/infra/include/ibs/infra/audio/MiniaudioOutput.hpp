#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "ibs/core/ports/AudioOutputPort.hpp"

namespace ibs::infra {

class MiniaudioOutput final : public core::AudioOutputPort {
public:
    explicit MiniaudioOutput(int preferredSampleRate = 48000);
    ~MiniaudioOutput() override;

    MiniaudioOutput(const MiniaudioOutput&) = delete;
    MiniaudioOutput& operator=(const MiniaudioOutput&) = delete;

    [[nodiscard]] int sampleRate() const override;
    bool start(core::AudioBuffer stereo) override;
    void stop() override;
    [[nodiscard]] bool isPlaying() const override;
    [[nodiscard]] core::SampleIndex playedFrames() const override;

    [[nodiscard]] const std::string& lastError() const noexcept { return lastError_; }

    struct Device;

private:
    bool ensureDevice();

    int preferredSampleRate_;
    std::unique_ptr<Device> device_;
    std::string lastError_;
};

}  // namespace ibs::infra
