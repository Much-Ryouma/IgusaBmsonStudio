#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

#include <gtest/gtest.h>

#include "ibs/core/slicing/Fft.hpp"

namespace ibs::core {
namespace {

TEST(Fft, FFT001_ImpulseHasFlatSpectrum) {
    std::vector<std::complex<double>> values(8, {0.0, 0.0});
    values[0] = {1.0, 0.0};

    fftInPlace(values);

    for (const auto& value : values) {
        EXPECT_NEAR(std::abs(value), 1.0, 1e-12);
    }
}

TEST(Fft, FFT002_SinePeaksAtItsBin) {
    std::vector<std::complex<double>> values(64);
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = std::sin(2.0 * std::numbers::pi * 4.0 * static_cast<double>(index) / 64.0);
    }

    fftInPlace(values);

    for (std::size_t bin = 0; bin < values.size(); ++bin) {
        const bool isPeak = bin == 4 || bin == 60;
        EXPECT_EQ(std::abs(values[bin]) > 1.0, isPeak) << "bin " << bin;
    }
}

TEST(Fft, FFT003_RejectsNonPowerOfTwoSize) {
    std::vector<std::complex<double>> values(6);

    EXPECT_THROW(fftInPlace(values), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
