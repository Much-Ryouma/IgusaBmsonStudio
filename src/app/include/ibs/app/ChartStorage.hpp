#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "ibs/core/bmson/BmsonDocument.hpp"
#include "ibs/core/ports/FileSystemPort.hpp"
#include "ibs/core/sidecar/Sidecar.hpp"
#include "ibs/core/validation/Validator.hpp"
#include "ibs/infra/bmson/BmsonJsonCodec.hpp"

namespace ibs::app {

class ChartStorage {
public:
    virtual ~ChartStorage() = default;

    [[nodiscard]] virtual infra::BmsonParseResult load(const std::filesystem::path& path) = 0;
    virtual void save(const std::filesystem::path& path, const core::bmson::Document& document) = 0;
    [[nodiscard]] virtual std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path& directory) = 0;

    // Why not 純粋仮想にする: 既存の差し替え用の実装（テストの偽物など）に、使わない機能の実装を強いないため
    [[nodiscard]] virtual std::optional<core::SidecarData> loadSidecar(const std::filesystem::path&) { return std::nullopt; }
    virtual void saveSidecar(const std::filesystem::path&, const core::SidecarData&) {}
    [[nodiscard]] virtual std::optional<core::ImageSize> imageSize(const std::filesystem::path&) { return std::nullopt; }
};

class LocalChartStorage final : public ChartStorage {
public:
    [[nodiscard]] infra::BmsonParseResult load(const std::filesystem::path& path) override;
    void save(const std::filesystem::path& path, const core::bmson::Document& document) override;
    [[nodiscard]] std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path& directory) override;
    [[nodiscard]] std::optional<core::SidecarData> loadSidecar(const std::filesystem::path& bmsonPath) override;
    void saveSidecar(const std::filesystem::path& bmsonPath, const core::SidecarData& data) override;
    [[nodiscard]] std::optional<core::ImageSize> imageSize(const std::filesystem::path& path) override;
};

}  // namespace ibs::app
