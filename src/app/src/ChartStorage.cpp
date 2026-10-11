#include "ibs/app/ChartStorage.hpp"

#include "ibs/infra/bmson/BmsonFileRepository.hpp"
#include "ibs/infra/fs/LocalFileSystem.hpp"
#include "ibs/infra/image/ImageSize.hpp"
#include "ibs/infra/sidecar/SidecarJsonCodec.hpp"

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

std::optional<core::SidecarData> LocalChartStorage::loadSidecar(const std::filesystem::path& bmsonPath) {
    return infra::loadSidecarFile(infra::sidecarPathFor(bmsonPath));
}

void LocalChartStorage::saveSidecar(const std::filesystem::path& bmsonPath, const core::SidecarData& data) {
    infra::saveSidecarFile(infra::sidecarPathFor(bmsonPath), data);
}

std::optional<core::ImageSize> LocalChartStorage::imageSize(const std::filesystem::path& path) {
    return infra::readImageSize(path);
}

}  // namespace ibs::app
