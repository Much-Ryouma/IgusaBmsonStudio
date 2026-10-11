#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include "ibs/core/sidecar/Sidecar.hpp"

namespace ibs::infra {

class SidecarFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] std::string serializeSidecar(const core::SidecarData& data);
[[nodiscard]] core::SidecarData parseSidecar(const std::string& text);

[[nodiscard]] std::filesystem::path sidecarPathFor(const std::filesystem::path& bmsonPath);
[[nodiscard]] std::optional<core::SidecarData> loadSidecarFile(const std::filesystem::path& sidecarPath);
void saveSidecarFile(const std::filesystem::path& sidecarPath, const core::SidecarData& data);

}  // namespace ibs::infra
