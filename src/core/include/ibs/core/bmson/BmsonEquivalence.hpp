#pragma once

#include <string>
#include <vector>

#include "ibs/core/bmson/BmsonDocument.hpp"

namespace ibs::core::bmson {

[[nodiscard]] SoundChannel normalizedForPlayback(const SoundChannel& channel);

[[nodiscard]] std::vector<std::string> describePlaybackDifferences(const Document& expected, const Document& actual);

}  // namespace ibs::core::bmson
