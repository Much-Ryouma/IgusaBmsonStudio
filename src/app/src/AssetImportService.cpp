#include "ibs/app/AssetImportService.hpp"

#include <exception>

#include "ibs/core/asset/AssetPath.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

bool LocalAssetFileStore::exists(const std::filesystem::path& path) const {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

void LocalAssetFileStore::copy(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::none);
}

std::optional<std::string> assetNameInside(const std::filesystem::path& chartDirectory, const std::filesystem::path& file) {
    const auto relative = file.lexically_normal().lexically_relative(chartDirectory.lexically_normal());
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        return std::nullopt;
    }
    // Why not OS の区切り文字のまま保存する: bmson は他の環境のプレーヤーでも読むため、区切りを / にそろえる
    const std::string name = core::normalizeSeparators(infra::pathToUtf8(relative));
    if (core::findUnsafePathReason(name)) {
        return std::nullopt;
    }
    return name;
}

AssetImportService::AssetImportService(AssetFileStore& files) : files_(files) {}

ImportOutcome AssetImportService::import(EditorDocument& document, const std::filesystem::path& file,
                                         OutsideFilePolicy policy) {
    ImportOutcome outcome;
    try {
        if (!document.path()) {
            outcome.error = "素材は譜面からの相対パスで保存するため、先に譜面の保存先を決めてください";
            return outcome;
        }
        const auto directory = document.chartDirectory();
        auto name = assetNameInside(directory, file);
        if (!name) {
            if (policy == OutsideFilePolicy::Reject) {
                outcome.error = "譜面のフォルダの外にある素材は使えません: " + infra::pathToUtf8(file);
                return outcome;
            }
            const auto target = directory / file.filename();
            name = infra::pathToUtf8(file.filename());
            // Why not 同名のファイルを上書きする: 別の曲の同名素材（kick.wav など）で、すでに使っている素材が置き換わるのを防ぐ
            if (files_.exists(target)) {
                const auto* existing = document.project().findAssetByName(*name);
                if (existing == nullptr) {
                    outcome.error = "譜面のフォルダに同じ名前のファイルがあります: " + *name;
                    return outcome;
                }
            } else {
                files_.copy(file, target);
                outcome.copied = true;
            }
        }
        outcome.assetName = *name;
        if (const auto* existing = document.project().findAssetByName(*name)) {
            outcome.assetId = existing->id;
            outcome.alreadyImported = true;
            return outcome;
        }
        outcome.assetId = document.apply("素材の追加", [&](core::Project& project) { return project.addAsset(*name); });
    } catch (const std::exception& error) {
        outcome.error = error.what();
    }
    return outcome;
}

}  // namespace ibs::app
