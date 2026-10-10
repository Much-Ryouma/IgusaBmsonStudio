#include "ibs/app/AudioLibrary.hpp"

#include <exception>

#include "ibs/core/asset/AssetPath.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

AudioLibrary::AudioLibrary(ChartStorage& storage, AudioSource& source) : storage_(storage), source_(source) {}

void AudioLibrary::setChartDirectory(const std::filesystem::path& directory) {
    std::lock_guard lock(mutex_);
    if (directory != directory_) {
        directory_ = directory;
        audio_.clear();
        errors_.clear();
        analyses_.clear();
    }
}

std::filesystem::path AudioLibrary::chartDirectory() const {
    std::lock_guard lock(mutex_);
    return directory_;
}

AudioLookup AudioLibrary::get(const std::string& assetName) {
    std::filesystem::path directory;
    {
        std::lock_guard lock(mutex_);
        if (const auto found = audio_.find(assetName); found != audio_.end()) {
            return AudioLookup{found->second, {}};
        }
        // Why not 失敗したら毎回読み直す: 描画のたびに見つからないファイルを探しに行き、画面が固まるため、
        // 失敗も覚えておき、invalidate されるまで再試行しない
        if (const auto failed = errors_.find(assetName); failed != errors_.end()) {
            return AudioLookup{nullptr, failed->second};
        }
        directory = directory_;
    }

    AudioLookup lookup;
    try {
        const auto fileSystem = storage_.fileSystemAt(directory);
        const auto resolved = core::resolveSoundFile(assetName, *fileSystem);
        if (!resolved) {
            lookup.error = "音素材 '" + assetName + "' が見つかりません";
        } else {
            const auto file = directory / infra::pathFromUtf8(*resolved);
            core::AudioBuffer audio = source_.load(file);
            std::vector<float> mono = core::mixToMono(audio);
            core::WaveformPyramid waveform(mono);
            lookup.audio = std::make_shared<const LoadedAudio>(
                LoadedAudio{assetName, file, std::move(audio), std::move(mono), std::move(waveform)});
        }
    } catch (const std::exception& error) {
        lookup.error = error.what();
    }

    std::lock_guard lock(mutex_);
    if (directory != directory_) {
        return lookup;
    }
    if (lookup.audio) {
        audio_[assetName] = lookup.audio;
    } else {
        errors_[assetName] = lookup.error;
    }
    return lookup;
}

std::shared_ptr<const LoadedAudio> AudioLibrary::cached(const std::string& assetName) const {
    std::lock_guard lock(mutex_);
    const auto found = audio_.find(assetName);
    return found == audio_.end() ? nullptr : found->second;
}

std::shared_ptr<const core::OnsetAnalysis> AudioLibrary::analysis(const std::string& assetName,
                                                                  const core::OnsetParameters& parameters) {
    if (auto existing = cachedAnalysis(assetName, parameters)) {
        return existing;
    }
    const auto lookup = get(assetName);
    if (!lookup.audio) {
        return nullptr;
    }
    auto computed = std::make_shared<const core::OnsetAnalysis>(
        core::analyzeOnsets(lookup.audio->mono, lookup.audio->audio.sampleRate, parameters));
    storeAnalysis(assetName, computed);
    return computed;
}

std::shared_ptr<const core::OnsetAnalysis> AudioLibrary::cachedAnalysis(const std::string& assetName,
                                                                        const core::OnsetParameters& parameters) const {
    std::lock_guard lock(mutex_);
    const auto found = analyses_.find(assetName);
    if (found == analyses_.end() || !core::analysisMatches(*found->second, parameters)) {
        return nullptr;
    }
    return found->second;
}

void AudioLibrary::storeAnalysis(const std::string& assetName, std::shared_ptr<const core::OnsetAnalysis> analysis) {
    std::lock_guard lock(mutex_);
    analyses_[assetName] = std::move(analysis);
}

void AudioLibrary::invalidate(const std::string& assetName) {
    std::lock_guard lock(mutex_);
    audio_.erase(assetName);
    errors_.erase(assetName);
    analyses_.erase(assetName);
}

void AudioLibrary::clear() {
    std::lock_guard lock(mutex_);
    audio_.clear();
    errors_.clear();
    analyses_.clear();
}

}  // namespace ibs::app
