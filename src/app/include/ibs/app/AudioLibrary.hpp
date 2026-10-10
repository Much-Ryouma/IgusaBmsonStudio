#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ibs/app/ChartStorage.hpp"
#include "ibs/app/SlicingService.hpp"
#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/audio/WaveformPyramid.hpp"
#include "ibs/core/slicing/OnsetDetector.hpp"

namespace ibs::app {

struct LoadedAudio {
    std::string assetName;
    std::filesystem::path file;
    core::AudioBuffer audio;
    std::vector<float> mono;
    core::WaveformPyramid waveform;
};

struct AudioLookup {
    std::shared_ptr<const LoadedAudio> audio;
    std::string error;
};

class AudioLibrary {
public:
    AudioLibrary(ChartStorage& storage, AudioSource& source);

    void setChartDirectory(const std::filesystem::path& directory);
    [[nodiscard]] std::filesystem::path chartDirectory() const;

    [[nodiscard]] AudioLookup get(const std::string& assetName);
    [[nodiscard]] std::shared_ptr<const LoadedAudio> cached(const std::string& assetName) const;

    [[nodiscard]] std::shared_ptr<const core::OnsetAnalysis> analysis(const std::string& assetName,
                                                                       const core::OnsetParameters& parameters);
    [[nodiscard]] std::shared_ptr<const core::OnsetAnalysis> cachedAnalysis(const std::string& assetName,
                                                                             const core::OnsetParameters& parameters) const;
    void storeAnalysis(const std::string& assetName, std::shared_ptr<const core::OnsetAnalysis> analysis);

    void invalidate(const std::string& assetName);
    void clear();

private:
    ChartStorage& storage_;
    AudioSource& source_;
    mutable std::mutex mutex_;
    std::filesystem::path directory_;
    std::map<std::string, std::shared_ptr<const LoadedAudio>> audio_;
    std::map<std::string, std::string> errors_;
    std::map<std::string, std::shared_ptr<const core::OnsetAnalysis>> analyses_;
};

}  // namespace ibs::app
