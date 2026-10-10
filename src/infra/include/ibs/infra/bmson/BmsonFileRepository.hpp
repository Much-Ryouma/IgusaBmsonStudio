#pragma once

#include <filesystem>

#include "ibs/core/bmson/BmsonDocument.hpp"
#include "ibs/infra/bmson/BmsonJsonCodec.hpp"

namespace ibs::infra {

[[nodiscard]] BmsonParseResult loadBmsonFile(const std::filesystem::path& path);

void saveBmsonFile(const std::filesystem::path& path, const core::bmson::Document& document);

}  // namespace ibs::infra
