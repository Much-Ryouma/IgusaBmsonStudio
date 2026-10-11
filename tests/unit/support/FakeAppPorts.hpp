#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "FakeFileSystem.hpp"
#include "ibs/app/AssetImportService.hpp"
#include "ibs/app/ChartStorage.hpp"
#include "ibs/app/PlaybackService.hpp"
#include "ibs/app/PreviewService.hpp"
#include "ibs/app/SlicingService.hpp"

namespace ibs::testing {

class FilesOnlyStorage final : public app::ChartStorage {
public:
    explicit FilesOnlyStorage(std::set<std::string> files = {}) : files_(std::move(files)) {}

    infra::BmsonParseResult load(const std::filesystem::path&) override { return {}; }
    void save(const std::filesystem::path&, const core::bmson::Document&) override {}
    std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path&) override {
        ++lookups;
        return std::make_unique<FakeFileSystem>(files_);
    }

    int lookups = 0;

private:
    std::set<std::string> files_;
};

class InMemoryAudioSource final : public app::AudioSource {
public:
    std::map<std::filesystem::path, core::AudioBuffer> buffers;
    std::vector<std::filesystem::path> loaded;

    core::AudioBuffer load(const std::filesystem::path& path) override {
        loaded.push_back(path);
        return buffers.at(path);
    }
};

class FakeAudioOutput final : public app::AudioOutput {
public:
    explicit FakeAudioOutput(int rate = 1000) : rate_(rate) {}

    int sampleRate() const override { return rate_; }
    bool start(core::AudioBuffer stereo) override {
        if (failStart) {
            return false;
        }
        played = std::move(stereo);
        playing = true;
        return true;
    }
    void stop() override { playing = false; }
    bool isPlaying() const override { return playing; }
    core::SampleIndex playedFrames() const override { return position; }

    bool failStart = false;
    bool playing = false;
    core::SampleIndex position = 0;
    core::AudioBuffer played;

private:
    int rate_;
};

class FakeAssetFileStore final : public app::AssetFileStore {
public:
    std::set<std::filesystem::path> files;
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> copies;

    bool exists(const std::filesystem::path& path) const override { return files.contains(path); }
    void copy(const std::filesystem::path& from, const std::filesystem::path& to) override {
        copies.emplace_back(from, to);
        files.insert(to);
    }
};

class RecordingLauncher final : public app::ProcessLauncher {
public:
    bool launch(const app::LaunchRequest& request) override {
        requests.push_back(request);
        return succeed;
    }

    bool succeed = true;
    std::vector<app::LaunchRequest> requests;
};

}  // namespace ibs::testing
