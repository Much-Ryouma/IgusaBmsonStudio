#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "ibs/core/validation/Validator.hpp"

namespace ibs::infra {

[[nodiscard]] std::optional<core::ImageSize> imageSizeFromHeader(const std::string& bytes);
[[nodiscard]] std::optional<core::ImageSize> readImageSize(const std::filesystem::path& path);

}  // namespace ibs::infra
