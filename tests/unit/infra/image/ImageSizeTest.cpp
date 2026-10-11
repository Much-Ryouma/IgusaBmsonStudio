#include <string>

#include <gtest/gtest.h>

#include "ibs/infra/image/ImageSize.hpp"

namespace ibs::infra {
namespace {

std::string bytes(std::initializer_list<int> values) {
    std::string result;
    for (const int value : values) {
        result.push_back(static_cast<char>(value));
    }
    return result;
}

TEST(ImageSize, IMG001_ReadsPngJpegAndBmpHeaders) {
    const auto png = bytes({0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R',
                            0, 0, 0x01, 0x2C, 0, 0, 0, 0x50});
    const auto jpeg = bytes({0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x04, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x11, 0x08,
                             0x00, 0x50, 0x01, 0x2C, 0x03, 0, 0, 0, 0, 0, 0});
    std::string bmp = bytes({'B', 'M'});
    bmp.resize(26, '\0');
    bmp[18] = static_cast<char>(0x2C);
    bmp[19] = 0x01;
    bmp[22] = static_cast<char>(0xB0);
    bmp[23] = static_cast<char>(0xFF);
    bmp[24] = static_cast<char>(0xFF);
    bmp[25] = static_cast<char>(0xFF);

    const auto pngSize = imageSizeFromHeader(png);
    const auto jpegSize = imageSizeFromHeader(jpeg);
    const auto bmpSize = imageSizeFromHeader(bmp);

    ASSERT_TRUE(pngSize && jpegSize && bmpSize);
    EXPECT_EQ(pngSize->width, 300);
    EXPECT_EQ(pngSize->height, 80);
    EXPECT_EQ(jpegSize->width, 300);
    EXPECT_EQ(jpegSize->height, 80);
    EXPECT_EQ(bmpSize->width, 300);
    EXPECT_EQ(bmpSize->height, 80);
}

TEST(ImageSize, IMG002_UnknownFormatsAndMissingFilesGiveNothing) {
    EXPECT_FALSE(imageSizeFromHeader("GIF89a......").has_value());
    EXPECT_FALSE(readImageSize("no-such-image.png").has_value());
}

}  // namespace
}  // namespace ibs::infra
