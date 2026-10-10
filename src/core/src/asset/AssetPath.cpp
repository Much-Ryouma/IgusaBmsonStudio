#include "ibs/core/asset/AssetPath.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace ibs::core {

namespace {

constexpr std::array<std::string_view, 3> kSoundExtensions = {".wav", ".ogg", ".m4a"};

bool startsWithDriveLetter(const std::string& path) {
    return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) != 0 && path[1] == ':';
}

std::vector<std::string> splitSegments(const std::string& normalizedPath) {
    std::vector<std::string> segments;
    std::string current;
    for (const char character : normalizedPath) {
        if (character == '/') {
            segments.push_back(current);
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    segments.push_back(current);
    return segments;
}

std::string withoutExtension(const std::string& normalizedPath) {
    const auto lastSlash = normalizedPath.find_last_of('/');
    const auto lastDot = normalizedPath.find_last_of('.');
    const bool dotInFileName = lastDot != std::string::npos && (lastSlash == std::string::npos || lastDot > lastSlash);
    return dotInFileName ? normalizedPath.substr(0, lastDot) : normalizedPath;
}

}  // namespace

std::string normalizeSeparators(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

std::optional<std::string> findUnsafePathReason(const std::string& path) {
    if (path.empty()) {
        return "ファイル名が空です";
    }
    if (path.find('\0') != std::string::npos) {
        return "NUL 文字を含んでいます";
    }
    const std::string normalized = normalizeSeparators(path);
    if (normalized.front() == '/' || startsWithDriveLetter(normalized)) {
        return "絶対パスです";
    }
    const auto segments = splitSegments(normalized);
    if (std::find(segments.begin(), segments.end(), "..") != segments.end()) {
        return "親フォルダ (..) を参照しています";
    }
    return std::nullopt;
}

std::vector<std::string> soundFileCandidates(const std::string& name) {
    const std::string normalized = normalizeSeparators(name);
    const std::string stem = withoutExtension(normalized);

    std::vector<std::string> candidates{normalized};
    for (const auto extension : kSoundExtensions) {
        std::string candidate = stem + std::string(extension);
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
            candidates.push_back(std::move(candidate));
        }
    }
    return candidates;
}

std::optional<std::string> resolveSoundFile(const std::string& name, const FileSystemPort& fileSystem) {
    if (findUnsafePathReason(name).has_value()) {
        return std::nullopt;
    }
    for (const auto& candidate : soundFileCandidates(name)) {
        if (fileSystem.exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

}  // namespace ibs::core
