#include "ibs/infra/image/ImageSize.hpp"

#include <cstdint>
#include <fstream>

namespace ibs::infra {

namespace {

constexpr std::size_t kHeaderBytes = 64 * 1024;

std::uint32_t bigEndian(const std::string& bytes, std::size_t at, int count) {
    std::uint32_t value = 0;
    for (int index = 0; index < count; ++index) {
        value = (value << 8) | static_cast<unsigned char>(bytes[at + static_cast<std::size_t>(index)]);
    }
    return value;
}

std::uint32_t littleEndian(const std::string& bytes, std::size_t at, int count) {
    std::uint32_t value = 0;
    for (int index = count - 1; index >= 0; --index) {
        value = (value << 8) | static_cast<unsigned char>(bytes[at + static_cast<std::size_t>(index)]);
    }
    return value;
}

std::optional<core::ImageSize> jpegSize(const std::string& bytes) {
    std::size_t at = 2;
    while (at + 9 < bytes.size()) {
        if (static_cast<unsigned char>(bytes[at]) != 0xFF) {
            return std::nullopt;
        }
        const auto marker = static_cast<unsigned char>(bytes[at + 1]);
        const std::uint32_t length = bigEndian(bytes, at + 2, 2);
        const bool startOfFrame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (startOfFrame) {
            return core::ImageSize{static_cast<int>(bigEndian(bytes, at + 7, 2)), static_cast<int>(bigEndian(bytes, at + 5, 2))};
        }
        at += 2 + length;
    }
    return std::nullopt;
}

}  // namespace

// Why not 画像ライブラリで読み込む: バナーの縦横比を調べるだけのために core や app に Qt や画像ライブラリを
// 持ち込みたくないため、BGA でよく使う png / jpeg / bmp のヘッダーだけを読む
std::optional<core::ImageSize> imageSizeFromHeader(const std::string& bytes) {
    if (bytes.size() >= 24 && bytes.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0) {
        return core::ImageSize{static_cast<int>(bigEndian(bytes, 16, 4)), static_cast<int>(bigEndian(bytes, 20, 4))};
    }
    if (bytes.size() >= 4 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xD8) {
        return jpegSize(bytes);
    }
    if (bytes.size() >= 26 && bytes.compare(0, 2, "BM") == 0) {
        const auto height = static_cast<std::int32_t>(littleEndian(bytes, 22, 4));
        return core::ImageSize{static_cast<int>(littleEndian(bytes, 18, 4)), height < 0 ? -height : height};
    }
    return std::nullopt;
}

std::optional<core::ImageSize> readImageSize(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::string bytes(kHeaderBytes, '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(file.gcount()));
    return imageSizeFromHeader(bytes);
}

}  // namespace ibs::infra
