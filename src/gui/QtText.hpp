#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <QString>

#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::gui {

inline QString toQString(std::string_view utf8) {
    return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
}

inline QString toQString(const std::string& utf8) { return toQString(std::string_view(utf8)); }

inline QString toQString(const char* utf8) { return toQString(std::string_view(utf8)); }

inline std::string toUtf8(const QString& text) {
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

inline std::filesystem::path toPath(const QString& text) { return infra::pathFromUtf8(toUtf8(text)); }

inline QString toQString(const std::filesystem::path& path) { return toQString(infra::pathToUtf8(path)); }

}  // namespace ibs::gui
