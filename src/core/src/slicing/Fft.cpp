#include "ibs/core/slicing/Fft.hpp"

#include <numbers>
#include <stdexcept>
#include <utility>

namespace ibs::core {

namespace {

// Why not std::complex の operator* を使う: 規格どおりの NaN/無限大処理が入るため、
// コンパイラによっては関数呼び出しになり、解析時間の大半を占めてしまう
std::complex<double> multiply(const std::complex<double>& a, const std::complex<double>& b) noexcept {
    return {a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real()};
}

std::size_t validatedHalf(std::size_t size) {
    if (size < 4 || (size & (size - 1)) != 0) {
        throw std::invalid_argument("real FFT size must be a power of two and at least 4");
    }
    return size / 2;
}

}  // namespace

bool isPowerOfTwo(int value) noexcept { return value > 0 && (value & (value - 1)) == 0; }

FftPlan::FftPlan(std::size_t size) : size_(size), bitReversed_(size), twiddles_(size / 2) {
    if (!isPowerOfTwo(static_cast<int>(size))) {
        throw std::invalid_argument("FFT size must be a power of two");
    }
    for (std::size_t index = 1, reversed = 0; index < size; ++index) {
        std::size_t bit = size >> 1;
        for (; (reversed & bit) != 0; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;
        bitReversed_[index] = reversed;
    }
    for (std::size_t index = 0; index < size / 2; ++index) {
        const double angle = -2.0 * std::numbers::pi * static_cast<double>(index) / static_cast<double>(size);
        twiddles_[index] = {std::cos(angle), std::sin(angle)};
    }
}

void FftPlan::transform(std::vector<std::complex<double>>& values) const {
    if (values.size() != size_) {
        throw std::invalid_argument("FFT input size does not match the plan");
    }
    for (std::size_t index = 1; index < size_; ++index) {
        if (index < bitReversed_[index]) {
            std::swap(values[index], values[bitReversed_[index]]);
        }
    }
    for (std::size_t length = 2; length <= size_; length <<= 1) {
        const std::size_t half = length / 2;
        const std::size_t twiddleStride = size_ / length;
        for (std::size_t start = 0; start < size_; start += length) {
            for (std::size_t offset = 0; offset < half; ++offset) {
                const auto even = values[start + offset];
                const auto odd = multiply(values[start + offset + half], twiddles_[offset * twiddleStride]);
                values[start + offset] = even + odd;
                values[start + offset + half] = even - odd;
            }
        }
    }
}

RealFftPlan::RealFftPlan(std::size_t size)
    : size_(size), half_(validatedHalf(size)), packed_(size / 2), twiddles_(size / 2 + 1) {
    for (std::size_t index = 0; index <= size / 2; ++index) {
        const double angle = -2.0 * std::numbers::pi * static_cast<double>(index) / static_cast<double>(size);
        twiddles_[index] = {std::cos(angle), std::sin(angle)};
    }
}

void RealFftPlan::transform(const std::vector<double>& input, std::vector<std::complex<double>>& spectrum) {
    if (input.size() != size_) {
        throw std::invalid_argument("FFT input size does not match the plan");
    }
    const std::size_t half = size_ / 2;
    for (std::size_t index = 0; index < half; ++index) {
        packed_[index] = {input[2 * index], input[2 * index + 1]};
    }
    half_.transform(packed_);
    spectrum.resize(half + 1);
    for (std::size_t bin = 0; bin <= half; ++bin) {
        const auto z = packed_[bin % half];
        const auto mirrored = std::conj(packed_[(half - bin) % half]);
        const std::complex<double> even{0.5 * (z.real() + mirrored.real()), 0.5 * (z.imag() + mirrored.imag())};
        const std::complex<double> oddTimesI{0.5 * (z.real() - mirrored.real()), 0.5 * (z.imag() - mirrored.imag())};
        const std::complex<double> odd{oddTimesI.imag(), -oddTimesI.real()};
        spectrum[bin] = even + multiply(odd, twiddles_[bin]);
    }
}

void fftInPlace(std::vector<std::complex<double>>& values) {
    if (!isPowerOfTwo(static_cast<int>(values.size()))) {
        throw std::invalid_argument("FFT size must be a power of two");
    }
    FftPlan(values.size()).transform(values);
}

}  // namespace ibs::core
