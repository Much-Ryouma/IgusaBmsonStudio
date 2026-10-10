#pragma once

namespace ibs::cli {

enum class ExitCode : int {
    Success = 0,
    ValidationError = 1,
    UsageError = 2,
    IoError = 3,
};

[[nodiscard]] constexpr int toInt(ExitCode code) noexcept {
    return static_cast<int>(code);
}

}  // namespace ibs::cli
