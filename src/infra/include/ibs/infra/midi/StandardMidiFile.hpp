#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ibs/core/midi/MidiSequence.hpp"

namespace ibs::infra {

[[nodiscard]] std::string encodeVariableLength(std::uint64_t value);

[[nodiscard]] std::string serializeStandardMidiFile(const core::MidiSequence& sequence);

[[nodiscard]] core::MidiProfile parseMidiProfile(const std::string& jsonText);

[[nodiscard]] std::vector<core::MidiProfile> loadMidiProfiles(const std::filesystem::path& directory);

}  // namespace ibs::infra
