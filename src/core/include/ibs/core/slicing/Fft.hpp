#pragma once

#include <complex>
#include <vector>

namespace ibs::core {

class FftPlan {
public:
    explicit FftPlan(std::size_t size);

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void transform(std::vector<std::complex<double>>& values) const;

private:
    std::size_t size_;
    std::vector<std::size_t> bitReversed_;
    std::vector<std::complex<double>> twiddles_;
};

class RealFftPlan {
public:
    explicit RealFftPlan(std::size_t size);

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void transform(const std::vector<double>& input, std::vector<std::complex<double>>& spectrum);

private:
    std::size_t size_;
    FftPlan half_;
    std::vector<std::complex<double>> packed_;
    std::vector<std::complex<double>> twiddles_;
};

void fftInPlace(std::vector<std::complex<double>>& values);

[[nodiscard]] bool isPowerOfTwo(int value) noexcept;

}  // namespace ibs::core
