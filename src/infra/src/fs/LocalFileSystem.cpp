#include "ibs/infra/fs/LocalFileSystem.hpp"

#include <system_error>
#include <utility>

#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::infra {

LocalFileSystem::LocalFileSystem(std::filesystem::path baseDirectory) : baseDirectory_(std::move(baseDirectory)) {}

bool LocalFileSystem::exists(const std::string& relativePath) const {
    std::error_code error;
    return std::filesystem::is_regular_file(baseDirectory_ / pathFromUtf8(relativePath), error);
}

}  // namespace ibs::infra
