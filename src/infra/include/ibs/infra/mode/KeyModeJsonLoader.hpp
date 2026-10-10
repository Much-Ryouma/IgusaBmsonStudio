#pragma once

#include <filesystem>
#include <string>

#include "ibs/core/mode/KeyMode.hpp"

namespace ibs::infra {

[[nodiscard]] core::KeyMode parseKeyMode(const std::string& jsonText);

[[nodiscard]] core::KeyModeCatalog loadKeyModeCatalog(const std::filesystem::path& directory);

}  // namespace ibs::infra
