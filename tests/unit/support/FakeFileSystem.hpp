#pragma once

#include <set>
#include <string>
#include <utility>

#include "ibs/core/ports/FileSystemPort.hpp"

namespace ibs::testing {

class FakeFileSystem final : public core::FileSystemPort {
public:
    explicit FakeFileSystem(std::set<std::string> files = {}) : files_(std::move(files)) {}

    [[nodiscard]] bool exists(const std::string& relativePath) const override { return files_.count(relativePath) > 0; }

private:
    std::set<std::string> files_;
};

}  // namespace ibs::testing
