#pragma once

#include <complex>
#include <vector>

namespace ibs::core {

void fftInPlace(std::vector<std::complex<double>>& values);

[[nodiscard]] bool isPowerOfTwo(int value) noexcept;

}  // namespace ibs::core
