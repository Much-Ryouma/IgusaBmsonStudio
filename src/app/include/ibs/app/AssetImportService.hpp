#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "ibs/app/EditorDocument.hpp"

namespace ibs::app {

class AssetFileStore {
public:
    virtual ~AssetFileStore() = default;

    [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;
    virtual void copy(const std::filesystem::path& from, const std::filesystem::path& to) = 0;
};

class LocalAssetFileStore final : public AssetFileStore {
public:
    [[nodiscard]] bool exists(const std::filesystem::path& path) const override;
    void copy(const std::filesystem::path& from, const std::filesystem::path& to) override;
};

enum class OutsideFilePolicy { Copy, Reject };

struct ImportOutcome {
    std::optional<core::AssetId> assetId;
    std::string assetName;
    bool copied = false;
    bool alreadyImported = false;
    std::string error;
};

struct BgaImportOutcome {
    std::optional<std::uint32_t> imageId;
    std::string name;
    bool copied = false;
    bool alreadyImported = false;
    std::string error;
};

[[nodiscard]] bool isBgaFile(const std::filesystem::path& file);

[[nodiscard]] std::optional<std::string> assetNameInside(const std::filesystem::path& chartDirectory,
                                                         const std::filesystem::path& file);

class AssetImportService {
public:
    explicit AssetImportService(AssetFileStore& files);

    ImportOutcome import(EditorDocument& document, const std::filesystem::path& file, OutsideFilePolicy policy);
    BgaImportOutcome importBga(EditorDocument& document, const std::filesystem::path& file, OutsideFilePolicy policy);

private:
    struct Placed {
        std::optional<std::string> name;
        bool copied = false;
        std::string error;
    };

    Placed placeInChartFolder(const EditorDocument& document, const std::filesystem::path& file, OutsideFilePolicy policy,
                              const std::string& kind, bool alreadyRegistered(const core::Project&, const std::string&));

    AssetFileStore& files_;
};

}  // namespace ibs::app
