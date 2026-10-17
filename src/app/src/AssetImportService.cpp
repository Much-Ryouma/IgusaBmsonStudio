#include "ibs/app/AssetImportService.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

#include "ibs/app/BgaCommands.hpp"
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

bool isBgaFile(const std::filesystem::path& file) {
    std::string extension = infra::pathToUtf8(file.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    for (const char* known : {".png", ".jpg", ".jpeg", ".bmp", ".webm", ".mp4", ".mpg", ".mpeg", ".wmv", ".avi"}) {
        if (extension == known) {
            return true;
        }
    }
    return false;
}

AssetImportService::Placed AssetImportService::placeInChartFolder(
    const EditorDocument& document, const std::filesystem::path& file, OutsideFilePolicy policy, const std::string& kind,
    bool alreadyRegistered(const core::Project&, const std::string&)) {
    Placed placed;
    if (!document.path()) {
        placed.error = kind + "は譜面からの相対パスで保存するため、先に譜面の保存先を決めてください";
        return placed;
    }
    const auto directory = document.chartDirectory();
    placed.name = assetNameInside(directory, file);
    if (placed.name) {
        return placed;
    }
    if (policy == OutsideFilePolicy::Reject) {
        placed.error = "譜面のフォルダの外にある" + kind + "は使えません: " + infra::pathToUtf8(file);
        return placed;
    }
    const auto target = directory / file.filename();
    const auto name = infra::pathToUtf8(file.filename());
    // Why not 同名のファイルを上書きする: 別の曲の同名素材（kick.wav など）で、すでに使っている素材が置き換わるのを防ぐ
    if (files_.exists(target)) {
        if (!alreadyRegistered(document.project(), name)) {
            placed.error = "譜面のフォルダに同じ名前のファイルがあります: " + name;
            return placed;
        }
    } else {
        files_.copy(file, target);
        placed.copied = true;
    }
    placed.name = name;
    return placed;
}

ImportOutcome AssetImportService::import(EditorDocument& document, const std::filesystem::path& file,
                                         OutsideFilePolicy policy) {
    ImportOutcome outcome;
    try {
        const auto placed = placeInChartFolder(document, file, policy, "素材",
                                               [](const core::Project& project, const std::string& name) {
                                                   return project.findAssetByName(name) != nullptr;
                                               });
        if (!placed.name) {
            outcome.error = placed.error;
            return outcome;
        }
        const auto& name = *placed.name;
        outcome.copied = placed.copied;
        outcome.assetName = name;
        if (const auto* existing = document.project().findAssetByName(name)) {
            outcome.assetId = existing->id;
            outcome.alreadyImported = true;
            return outcome;
        }
        outcome.assetId = document.apply("素材の追加", [&](core::Project& project) { return project.addAsset(name); });
    } catch (const std::exception& error) {
        outcome.error = error.what();
    }
    return outcome;
}

BgaImportOutcome AssetImportService::importBga(EditorDocument& document, const std::filesystem::path& file,
                                               OutsideFilePolicy policy) {
    const auto registered = [](const core::Project& project, const std::string& name) {
        return std::any_of(project.bga.headers.begin(), project.bga.headers.end(),
                           [&name](const core::BgaHeader& header) { return header.name == name; });
    };
    BgaImportOutcome outcome;
    try {
        if (!isBgaFile(file)) {
            outcome.error = "BGA に使える画像・動画ではありません: " + infra::pathToUtf8(file.filename());
            return outcome;
        }
        const auto placed = placeInChartFolder(document, file, policy, "BGA", registered);
        if (!placed.name) {
            outcome.error = placed.error;
            return outcome;
        }
        outcome.name = *placed.name;
        outcome.copied = placed.copied;
        outcome.alreadyImported = registered(document.project(), outcome.name);
        outcome.imageId = addBgaImage(document, outcome.name);
    } catch (const std::exception& error) {
        outcome.error = error.what();
    }
    return outcome;
}

}  // namespace ibs::app
