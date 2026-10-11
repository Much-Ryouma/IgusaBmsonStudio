#include "ibs/app/ProjectAudio.hpp"

#include <exception>

#include "ibs/core/asset/AssetPath.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

std::map<std::string, const core::AudioBuffer*> ProjectAudio::byName() const {
    std::map<std::string, const core::AudioBuffer*> result;
    for (const auto& [name, buffer] : buffers) {
        result.emplace(name, &buffer);
    }
    return result;
}

ProjectAudio loadProjectAudio(const core::Project& project, const std::filesystem::path& chartDirectory,
                              ChartStorage& storage, AudioSource& audio) {
    ProjectAudio loaded;
    const auto fileSystem = storage.fileSystemAt(chartDirectory);
    for (const auto& asset : project.assets()) {
        const auto resolved = fileSystem ? core::resolveSoundFile(asset.name, *fileSystem) : std::nullopt;
        if (!resolved) {
            loaded.missing.push_back(asset.name);
            continue;
        }
        try {
            loaded.buffers.emplace(asset.name, audio.load(chartDirectory / infra::pathFromUtf8(*resolved)));
        } catch (const std::exception&) {
            loaded.missing.push_back(asset.name);
        }
    }
    return loaded;
}

}  // namespace ibs::app
