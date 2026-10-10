#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ibs/core/ports/FileSystemPort.hpp"

namespace ibs::core {

[[nodiscard]] std::string normalizeSeparators(const std::string& path);

[[nodiscard]] std::optional<std::string> findUnsafePathReason(const std::string& path);

[[nodiscard]] std::vector<std::string> soundFileCandidates(const std::string& name);

[[nodiscard]] std::optional<std::string> resolveSoundFile(const std::string& name, const FileSystemPort& fileSystem);

}  // namespace ibs::core
