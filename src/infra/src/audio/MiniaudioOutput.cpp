#include "ibs/infra/audio/MiniaudioOutput.hpp"

#include <algorithm>
#include <cstring>

#include <miniaudio.h>

namespace ibs::infra {

namespace {

constexpr ma_uint32 kChannels = 2;
constexpr ma_uint32 kPeriodMilliseconds = 10;

}  // namespace

struct MiniaudioOutput::Device {
    ma_device device{};
    bool initialized = false;
    std::mutex bufferMutex;
    core::AudioBuffer buffer;
    std::atomic<core::SampleIndex> position{0};
    std::atomic<bool> playing{false};

    static void onData(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
        auto* self = static_cast<Device*>(device->pUserData);
        auto* out = static_cast<float*>(output);
        std::memset(out, 0, static_cast<std::size_t>(frameCount) * kChannels * sizeof(float));
        // Why not ロックを待つ: 音声スレッドが GUI スレッドの処理を待つと音切れになるため、取れなければその周期は無音にする
        std::unique_lock lock(self->bufferMutex, std::try_to_lock);
        if (!lock.owns_lock() || !self->playing.load()) {
            return;
        }
        const auto total = static_cast<core::SampleIndex>(self->buffer.frameCount());
        const core::SampleIndex begin = self->position.load();
        const core::SampleIndex count = std::min<core::SampleIndex>(frameCount, std::max<core::SampleIndex>(0, total - begin));
        if (count > 0) {
            std::memcpy(out, self->buffer.interleavedSamples.data() + begin * kChannels,
                        static_cast<std::size_t>(count) * kChannels * sizeof(float));
        }
        self->position.store(begin + count);
        if (begin + count >= total) {
            self->playing.store(false);
        }
    }
};

MiniaudioOutput::MiniaudioOutput(int preferredSampleRate)
    : preferredSampleRate_(preferredSampleRate), device_(std::make_unique<Device>()) {}

MiniaudioOutput::~MiniaudioOutput() {
    if (device_->initialized) {
        ma_device_uninit(&device_->device);
    }
}

// Why not 起動時にデバイスを開く: 試聴しない作業（CLI、テスト、譜面の閲覧）でも音声デバイスを占有し、
// デバイスがない環境では起動自体が失敗するため、最初の再生で開く
bool MiniaudioOutput::ensureDevice() {
    if (device_->initialized) {
        return true;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = kChannels;
    config.sampleRate = static_cast<ma_uint32>(preferredSampleRate_);
    config.periodSizeInMilliseconds = kPeriodMilliseconds;
    config.performanceProfile = ma_performance_profile_low_latency;
    config.dataCallback = &Device::onData;
    config.pUserData = device_.get();
    if (ma_device_init(nullptr, &config, &device_->device) != MA_SUCCESS) {
        lastError_ = "音声出力デバイスを開けません";
        return false;
    }
    // Why not 再生のたびにデバイスを開始・停止する: 開始のたびに数十 ms の遅れが出て、スライス試聴の目標（30 ms）を超えるため、
    // 開いた後は動かし続けて無音を流す
    if (ma_device_start(&device_->device) != MA_SUCCESS) {
        ma_device_uninit(&device_->device);
        lastError_ = "音声出力デバイスを開始できません";
        return false;
    }
    device_->initialized = true;
    return true;
}

int MiniaudioOutput::sampleRate() const {
    return device_->initialized ? static_cast<int>(device_->device.sampleRate) : preferredSampleRate_;
}

bool MiniaudioOutput::start(core::AudioBuffer stereo) {
    if (!ensureDevice()) {
        return false;
    }
    if (stereo.channels != static_cast<int>(kChannels)) {
        lastError_ = "ステレオ以外の音は再生できません";
        return false;
    }
    std::lock_guard lock(device_->bufferMutex);
    device_->buffer = std::move(stereo);
    device_->position.store(0);
    device_->playing.store(device_->buffer.frameCount() > 0);
    return true;
}

void MiniaudioOutput::stop() { device_->playing.store(false); }

bool MiniaudioOutput::isPlaying() const { return device_->playing.load(); }

core::SampleIndex MiniaudioOutput::playedFrames() const { return device_->position.load(); }

}  // namespace ibs::infra
