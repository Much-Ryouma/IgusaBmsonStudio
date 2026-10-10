#include "ibs/app/ChartStorage.hpp"

#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/LocalFileSystem.hpp"

namespace ibs::app {

infra::BmsonParseResult LocalChartStorage::load(const std::filesystem::path& path) {
    return infra::loadBmsonFile(path);
}

void LocalChartStorage::save(const std::filesystem::path& path, const core::bmson::Document& document) {
    infra::saveBmsonFile(path, document);
}

std::unique_ptr<core::FileSystemPort> LocalChartStorage::fileSystemAt(const std::filesystem::path& directory) {
    return std::make_unique<infra::LocalFileSystem>(directory);
}

}  // namespace ibs::app
