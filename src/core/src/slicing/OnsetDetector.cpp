#include "ibs/core/slicing/OnsetDetector.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <string>

#include "ibs/core/slicing/Fft.hpp"

namespace ibs::core {

namespace {

constexpr double kLogCompression = 1000.0;
constexpr double kEnergyFloor = 1e-10;
constexpr double kMedianWindowSeconds = 0.1;
constexpr int kLocalMaximumRadiusFrames = 2;
constexpr std::size_t kEnvelopeBlock = 16;
constexpr std::size_t kEnvelopeWindowBlocks = 4;
constexpr std::size_t kLevelWindowBlocks = 32;
constexpr double kAttackRiseFraction = 0.2;
constexpr double kMinimumAttackRiseRatio = 2.0;

void requireValid(const OnsetParameters& parameters) {
    if (!isPowerOfTwo(parameters.frameSize) || parameters.frameSize < 64) {
        throw std::invalid_argument("frameSize must be a power of two and at least 64");
    }
    if (parameters.hopSize <= 0 || parameters.hopSize > parameters.frameSize) {
        throw std::invalid_argument("hopSize must be in (0, frameSize]");
    }
    if (parameters.sensitivity < 0.0 || parameters.sensitivity > 1.0) {
        throw std::invalid_argument("sensitivity must be in [0, 1]");
    }
    if (parameters.minimumIntervalMs < 0.0 || parameters.prerollMs < 0.0) {
        throw std::invalid_argument("minimumIntervalMs and prerollMs must not be negative");
    }
}

std::vector<double> hannWindow(int size) {
    std::vector<double> window(static_cast<std::size_t>(size));
    for (int index = 0; index < size; ++index) {
        window[static_cast<std::size_t>(index)] =
            0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * index / static_cast<double>(size));
    }
    return window;
}

std::size_t frameCountFor(std::size_t samples, const OnsetParameters& parameters) {
    const auto hop = static_cast<std::size_t>(parameters.hopSize);
    return samples == 0 ? 0 : (samples + hop - 1) / hop;
}

double sampleAt(const std::vector<float>& mono, std::size_t index) {
    return index < mono.size() ? static_cast<double>(mono[index]) : 0.0;
}

double frameRmsDb(const std::vector<float>& mono, std::size_t start, std::size_t size) {
    double sum = 0.0;
    for (std::size_t offset = 0; offset < size; ++offset) {
        const double value = sampleAt(mono, start + offset);
        sum += value * value;
    }
    return 10.0 * std::log10(sum / static_cast<double>(size) + kEnergyFloor);
}

std::vector<double> magnitudeSpectrum(const std::vector<float>& mono, std::size_t start,
                                      const std::vector<double>& window) {
    std::vector<std::complex<double>> spectrum(window.size());
    for (std::size_t offset = 0; offset < window.size(); ++offset) {
        spectrum[offset] = sampleAt(mono, start + offset) * window[offset];
    }
    fftInPlace(spectrum);
    std::vector<double> magnitudes(window.size() / 2 + 1);
    const double scale = 2.0 / static_cast<double>(window.size());
    for (std::size_t bin = 0; bin < magnitudes.size(); ++bin) {
        magnitudes[bin] = std::abs(spectrum[bin]) * scale;
    }
    return magnitudes;
}

double positiveDifferenceSum(const std::vector<double>& current, const std::vector<double>& previous) {
    double sum = 0.0;
    for (std::size_t bin = 0; bin < current.size(); ++bin) {
        sum += std::max(0.0, current[bin] - previous[bin]);
    }
    return sum;
}

double highFrequencyContent(const std::vector<double>& magnitudes) {
    double sum = 0.0;
    for (std::size_t bin = 0; bin < magnitudes.size(); ++bin) {
        sum += static_cast<double>(bin) * magnitudes[bin] * magnitudes[bin];
    }
    return sum;
}

std::vector<double> frameRmsDbs(const std::vector<float>& mono, const OnsetParameters& parameters) {
    std::vector<double> levels(frameCountFor(mono.size(), parameters));
    for (std::size_t frame = 0; frame < levels.size(); ++frame) {
        levels[frame] = frameRmsDb(mono, frame * static_cast<std::size_t>(parameters.hopSize),
                                   static_cast<std::size_t>(parameters.frameSize));
    }
    return levels;
}

void normalizeToUnitPeak(std::vector<double>& values) {
    const double peak = values.empty() ? 0.0 : *std::max_element(values.begin(), values.end());
    if (peak > 0.0) {
        for (auto& value : values) {
            value /= peak;
        }
    }
}

double medianOf(std::vector<double> values) {
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

double thresholdOffsetFor(double sensitivity) {
    return 0.01 + 0.3 * (1.0 - sensitivity);
}

bool isLocalMaximum(const std::vector<double>& envelope, std::size_t frame) {
    const std::size_t begin = frame >= kLocalMaximumRadiusFrames ? frame - kLocalMaximumRadiusFrames : 0;
    const std::size_t end = std::min(envelope.size(), frame + kLocalMaximumRadiusFrames + 1);
    for (std::size_t other = begin; other < end; ++other) {
        if (other < frame && envelope[other] >= envelope[frame]) {
            return false;
        }
        if (other > frame && envelope[other] > envelope[frame]) {
            return false;
        }
    }
    return envelope[frame] > 0.0;
}

std::vector<double> blockEnvelope(const std::vector<float>& mono, std::size_t windowBlocks) {
    std::vector<double> blockEnergy((mono.size() + kEnvelopeBlock - 1) / kEnvelopeBlock, 0.0);
    for (std::size_t index = 0; index < mono.size(); ++index) {
        blockEnergy[index / kEnvelopeBlock] += static_cast<double>(mono[index]) * mono[index];
    }
    std::vector<double> envelope(blockEnergy.size(), 0.0);
    double running = 0.0;
    for (std::size_t block = 0; block < blockEnergy.size(); ++block) {
        running += blockEnergy[block];
        if (block >= windowBlocks) {
            running -= blockEnergy[block - windowBlocks];
        }
        envelope[block] = running;
    }
    return envelope;
}

SampleIndex spectralChangePosition(std::size_t frame, const OnsetParameters& parameters) {
    const auto hop = static_cast<SampleIndex>(parameters.hopSize);
    return static_cast<SampleIndex>(frame) * hop + parameters.frameSize / 2 - hop / 2;
}

struct Envelopes {
    std::vector<double> attack;
    std::vector<double> level;
};

bool hasLevelJump(const std::vector<double>& level, std::size_t begin, std::size_t end) {
    const auto peak = std::max_element(level.begin() + static_cast<std::ptrdiff_t>(begin),
                                       level.begin() + static_cast<std::ptrdiff_t>(end));
    const auto valley = std::min_element(level.begin() + static_cast<std::ptrdiff_t>(begin), peak + 1);
    return *peak >= kMinimumAttackRiseRatio * *valley && *peak > 0.0;
}

SampleIndex refineToAttackStart(const Envelopes& envelopes, std::size_t frame, const OnsetParameters& parameters) {
    const auto& envelope = envelopes.attack;
    const std::size_t searchBegin = frame * static_cast<std::size_t>(parameters.hopSize) / kEnvelopeBlock;
    const std::size_t searchEnd = std::min(
        envelope.size(),
        (frame * static_cast<std::size_t>(parameters.hopSize) + static_cast<std::size_t>(parameters.frameSize)) /
                kEnvelopeBlock + kEnvelopeWindowBlocks);
    if (searchBegin >= searchEnd) {
        return spectralChangePosition(frame, parameters);
    }

    // Why not 常に音量の立ち上がりで位置を決める: レガートで音程だけが変わる箇所は音量がほぼ変わらず、
    // 波形の揺れを立ち上がりと誤認して 10 ms 以上ずれるため、音量が十分に上がっていない場合はスペクトルの変化位置を使う
    // Why not 短い窓 (64 サンプル) で音量の変化を判定する: 低音の 1 周期より短く、波の山谷だけで「音量が上がった」と判定してしまうため
    const std::size_t levelEnd = std::min(envelopes.level.size(), searchEnd + kLevelWindowBlocks);
    if (!hasLevelJump(envelopes.level, searchBegin, levelEnd)) {
        return spectralChangePosition(frame, parameters);
    }
    const auto peak = std::max_element(envelope.begin() + static_cast<std::ptrdiff_t>(searchBegin),
                                       envelope.begin() + static_cast<std::ptrdiff_t>(searchEnd));
    const auto valley = std::min_element(envelope.begin() + static_cast<std::ptrdiff_t>(searchBegin), peak + 1);
    const double riseLevel = *valley + kAttackRiseFraction * (*peak - *valley);
    const auto rise = std::find_if(valley, peak + 1, [riseLevel](double level) { return level >= riseLevel; });

    // Why not 上昇を始めたブロックの末尾を使う: 包絡は過去 kEnvelopeWindowBlocks ブロック分の和なので、
    // 値が上がり始めたブロックの「先頭」がアタックの開始に最も近い
    const auto block = static_cast<std::size_t>(std::distance(envelope.begin(), rise));
    return static_cast<SampleIndex>(block * kEnvelopeBlock);
}

std::vector<Onset> enforceMinimumInterval(std::vector<Onset> onsets, SampleIndex minimumGap) {
    std::vector<Onset> kept;
    for (const auto& onset : onsets) {
        if (!kept.empty() && onset.sample - kept.back().sample < minimumGap) {
            if (onset.strength > kept.back().strength) {
                kept.back() = onset;
            }
            continue;
        }
        kept.push_back(onset);
    }
    return kept;
}

}  // namespace

std::vector<double> onsetStrengthEnvelope(const std::vector<float>& mono, const OnsetParameters& parameters) {
    requireValid(parameters);
    const std::size_t frames = frameCountFor(mono.size(), parameters);
    const auto hop = static_cast<std::size_t>(parameters.hopSize);
    const auto frameSize = static_cast<std::size_t>(parameters.frameSize);
    std::vector<double> envelope(frames, 0.0);

    if (parameters.function == OnsetFunction::Energy) {
        double previous = std::log(kEnergyFloor);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const double current = std::log(std::pow(10.0, frameRmsDb(mono, frame * hop, frameSize) / 10.0));
            envelope[frame] = std::max(0.0, current - previous);
            previous = current;
        }
    } else {
        const auto window = hannWindow(parameters.frameSize);
        std::vector<double> previous(frameSize / 2 + 1, 0.0);
        double previousHfc = 0.0;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            auto magnitudes = magnitudeSpectrum(mono, frame * hop, window);
            if (parameters.function == OnsetFunction::SpectralFlux) {
                for (auto& magnitude : magnitudes) {
                    magnitude = std::log1p(kLogCompression * magnitude);
                }
                envelope[frame] = positiveDifferenceSum(magnitudes, previous);
                previous = std::move(magnitudes);
            } else {
                const double hfc = highFrequencyContent(magnitudes);
                envelope[frame] = std::max(0.0, hfc - previousHfc);
                previousHfc = hfc;
            }
        }
    }
    normalizeToUnitPeak(envelope);
    return envelope;
}

std::vector<Onset> detectOnsets(const std::vector<float>& mono, int sampleRate, const OnsetParameters& parameters) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sampleRate must be positive");
    }
    const auto envelope = onsetStrengthEnvelope(mono, parameters);
    const auto levels = frameRmsDbs(mono, parameters);
    const Envelopes envelopes{blockEnvelope(mono, kEnvelopeWindowBlocks), blockEnvelope(mono, kLevelWindowBlocks)};

    const double hopSeconds = static_cast<double>(parameters.hopSize) / sampleRate;
    const auto medianRadius = static_cast<std::size_t>(std::max(1.0, std::round(kMedianWindowSeconds / hopSeconds)));
    const double offset = thresholdOffsetFor(parameters.sensitivity);
    const auto preroll = static_cast<SampleIndex>(std::llround(parameters.prerollMs * sampleRate / 1000.0));

    std::vector<Onset> onsets;
    for (std::size_t frame = 0; frame < envelope.size(); ++frame) {
        if (!isLocalMaximum(envelope, frame)) {
            continue;
        }
        const std::size_t begin = frame >= medianRadius ? frame - medianRadius : 0;
        const std::size_t end = std::min(envelope.size(), frame + medianRadius + 1);
        const double threshold =
            medianOf(std::vector<double>(envelope.begin() + static_cast<std::ptrdiff_t>(begin),
                                         envelope.begin() + static_cast<std::ptrdiff_t>(end))) +
            offset;
        if (envelope[frame] < threshold) {
            continue;
        }
        const double level = std::max(levels[frame], frame + 1 < levels.size() ? levels[frame + 1] : levels[frame]);
        if (level < parameters.silenceGateDb) {
            continue;
        }
        const SampleIndex attack = refineToAttackStart(envelopes, frame, parameters);
        onsets.push_back(Onset{std::max<SampleIndex>(0, attack - preroll), envelope[frame]});
    }

    std::sort(onsets.begin(), onsets.end(), [](const Onset& a, const Onset& b) { return a.sample < b.sample; });
    const auto minimumGap = static_cast<SampleIndex>(std::llround(parameters.minimumIntervalMs * sampleRate / 1000.0));
    return enforceMinimumInterval(std::move(onsets), minimumGap);
}

}  // namespace ibs::core
