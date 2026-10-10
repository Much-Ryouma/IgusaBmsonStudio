#pragma once

#include <filesystem>
#include <memory>

#include "ibs/core/bmson/BmsonDocument.hpp"
#include "ibs/core/ports/FileSystemPort.hpp"
#include "ibs/infra/bmson/BmsonJsonCodec.hpp"

namespace ibs::app {

class ChartStorage {
public:
    virtual ~ChartStorage() = default;

    [[nodiscard]] virtual infra::BmsonParseResult load(const std::filesystem::path& path) = 0;
    virtual void save(const std::filesystem::path& path, const core::bmson::Document& document) = 0;
    [[nodiscard]] virtual std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path& directory) = 0;
};

class LocalChartStorage final : public ChartStorage {
public:
    [[nodiscard]] infra::BmsonParseResult load(const std::filesystem::path& path) override;
    void save(const std::filesystem::path& path, const core::bmson::Document& document) override;
    [[nodiscard]] std::unique_ptr<core::FileSystemPort> fileSystemAt(const std::filesystem::path& directory) override;
};

}  // namespace ibs::app
