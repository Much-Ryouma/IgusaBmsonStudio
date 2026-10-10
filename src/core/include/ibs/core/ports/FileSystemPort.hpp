#pragma once

#include <string>

namespace ibs::core {

class FileSystemPort {
public:
    virtual ~FileSystemPort() = default;

    [[nodiscard]] virtual bool exists(const std::string& relativePath) const = 0;
};

}  // namespace ibs::core
