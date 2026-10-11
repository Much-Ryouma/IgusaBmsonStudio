#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace ibs::app {

inline constexpr std::size_t kRecentFileLimit = 10;

[[nodiscard]] std::vector<std::string> withRecentFile(std::vector<std::string> recent, const std::filesystem::path& file,
                                                      std::size_t limit = kRecentFileLimit);
[[nodiscard]] std::vector<std::string> withoutRecentFile(std::vector<std::string> recent, const std::filesystem::path& file);

}  // namespace ibs::app
