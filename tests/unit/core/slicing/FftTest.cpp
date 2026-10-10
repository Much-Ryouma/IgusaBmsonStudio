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

TEST(Fft, FFT004_RealFftMatchesComplexFftUpToNyquist) {
    for (const std::size_t size : {std::size_t{4}, std::size_t{8}, std::size_t{1024}}) {
        std::vector<double> input(size);
        std::vector<std::complex<double>> reference(size);
        for (std::size_t index = 0; index < size; ++index) {
            input[index] = std::sin(0.37 * static_cast<double>(index * index)) + 0.25 * std::cos(1.3 * static_cast<double>(index));
            reference[index] = input[index];
        }
        fftInPlace(reference);
        RealFftPlan plan(size);
        std::vector<std::complex<double>> spectrum;

        plan.transform(input, spectrum);

        ASSERT_EQ(spectrum.size(), size / 2 + 1);
        for (std::size_t bin = 0; bin <= size / 2; ++bin) {
            EXPECT_NEAR(std::abs(spectrum[bin] - reference[bin]), 0.0, 1e-9) << "size " << size << " bin " << bin;
        }
    }
}

TEST(Fft, FFT005_RealFftRejectsInvalidSizes) {
    EXPECT_THROW(RealFftPlan(2), std::invalid_argument);
    EXPECT_THROW(RealFftPlan(12), std::invalid_argument);
    RealFftPlan plan(8);
    std::vector<std::complex<double>> spectrum;
    EXPECT_THROW(plan.transform(std::vector<double>(4), spectrum), std::invalid_argument);
}

}  // namespace
}  // namespace ibs::core
