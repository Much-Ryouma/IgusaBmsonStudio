#include "ibs/core/slicing/Fft.hpp"

#include <numbers>
#include <stdexcept>
#include <utility>

namespace ibs::core {

bool isPowerOfTwo(int value) noexcept { return value > 0 && (value & (value - 1)) == 0; }

void fftInPlace(std::vector<std::complex<double>>& values) {
    const std::size_t size = values.size();
    if (!isPowerOfTwo(static_cast<int>(size))) {
        throw std::invalid_argument("FFT size must be a power of two");
    }

    for (std::size_t index = 1, reversed = 0; index < size; ++index) {
        std::size_t bit = size >> 1;
        for (; (reversed & bit) != 0; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(values[index], values[reversed]);
        }
    }

    for (std::size_t length = 2; length <= size; length <<= 1) {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> unitStep(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < size; start += length) {
            std::complex<double> twiddle(1.0, 0.0);
            for (std::size_t offset = 0; offset < length / 2; ++offset) {
                const auto even = values[start + offset];
                const auto odd = values[start + offset + length / 2] * twiddle;
                values[start + offset] = even + odd;
                values[start + offset + length / 2] = even - odd;
                twiddle *= unitStep;
            }
        }
    }
}

}  // namespace ibs::core
