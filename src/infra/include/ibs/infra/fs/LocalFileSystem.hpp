#pragma once

#include <filesystem>
#include <string>

#include "ibs/core/ports/FileSystemPort.hpp"

namespace ibs::infra {

class LocalFileSystem final : public core::FileSystemPort {
public:
    explicit LocalFileSystem(std::filesystem::path baseDirectory);

    [[nodiscard]] bool exists(const std::string& relativePath) const override;

private:
    std::filesystem::path baseDirectory_;
};

}  // namespace ibs::infra
