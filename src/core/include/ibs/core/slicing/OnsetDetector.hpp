#pragma once

#include <vector>

#include "ibs/core/audio/AudioBuffer.hpp"

namespace ibs::core {

enum class OnsetFunction { SpectralFlux, Energy, HighFrequencyContent };

struct OnsetParameters {
    OnsetFunction function = OnsetFunction::SpectralFlux;
    double sensitivity = 0.5;
    double minimumIntervalMs = 30.0;
    double silenceGateDb = -60.0;
    double prerollMs = 0.0;
    int frameSize = 1024;
    int hopSize = 256;
};

struct Onset {
    SampleIndex sample = 0;
    double strength = 0.0;
};

[[nodiscard]] std::vector<double> onsetStrengthEnvelope(const std::vector<float>& mono, const OnsetParameters& parameters);

[[nodiscard]] std::vector<Onset> detectOnsets(const std::vector<float>& mono, int sampleRate,
                                              const OnsetParameters& parameters);

}  // namespace ibs::core
