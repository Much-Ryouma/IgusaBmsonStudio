#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ibs/core/audio/AudioBuffer.hpp"
#include "ibs/core/bmson/BmsonDocument.hpp"

namespace ibs::core {

using NoteFilter = std::function<bool(const bmson::Note&)>;

[[nodiscard]] AudioBuffer renderBmson(const bmson::Document& document,
                                      const std::map<std::string, const AudioBuffer*>& audioByChannel,
                                      int outputSampleRate, const NoteFilter& include = {});

struct RenderComparisonOptions {
    double maxOffsetMs = 2000.0;
    double windowSeconds = 0.25;
    double silenceFloorDb = -60.0;
};

struct ResidualWindow {
    double startSeconds = 0.0;
    double residualDb = 0.0;
};

struct RenderComparison {
    double offsetSeconds = 0.0;
    double gain = 1.0;
    double residualDb = 0.0;
    std::vector<ResidualWindow> windows;
};

[[nodiscard]] RenderComparison compareRenders(const AudioBuffer& reference, const AudioBuffer& candidate,
                                              const RenderComparisonOptions& options = {});

}  // namespace ibs::core
