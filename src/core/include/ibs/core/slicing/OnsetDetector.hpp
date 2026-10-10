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

struct OnsetAnalysis {
    int sampleRate = 0;
    OnsetFunction function = OnsetFunction::SpectralFlux;
    int frameSize = 0;
    int hopSize = 0;
    std::vector<double> strength;
    std::vector<double> frameLevelsDb;
    std::vector<double> attackEnvelope;
    std::vector<double> levelEnvelope;
};

[[nodiscard]] OnsetAnalysis analyzeOnsets(const std::vector<float>& mono, int sampleRate,
                                          const OnsetParameters& parameters);

[[nodiscard]] bool analysisMatches(const OnsetAnalysis& analysis, const OnsetParameters& parameters) noexcept;

[[nodiscard]] std::vector<Onset> pickOnsets(const OnsetAnalysis& analysis, const OnsetParameters& parameters);

[[nodiscard]] std::vector<double> onsetStrengthEnvelope(const std::vector<float>& mono, const OnsetParameters& parameters);

[[nodiscard]] std::vector<Onset> detectOnsets(const std::vector<float>& mono, int sampleRate,
                                              const OnsetParameters& parameters);

}  // namespace ibs::core
