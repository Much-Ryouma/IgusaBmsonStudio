#pragma once

#include <filesystem>
#include <stdexcept>

#include "ibs/core/audio/AudioBuffer.hpp"

namespace ibs::infra {

class AudioDecodeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] core::AudioBuffer decodeAudioFile(const std::filesystem::path& path);

}  // namespace ibs::infra
