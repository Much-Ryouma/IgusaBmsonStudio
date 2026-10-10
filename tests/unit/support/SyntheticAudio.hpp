#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

namespace ibs::testing {

inline constexpr int kTestSampleRate = 44100;

inline std::size_t samplesAt(double seconds, int sampleRate = kTestSampleRate) {
    return static_cast<std::size_t>(std::llround(seconds * sampleRate));
}

class DeterministicNoise {
public:
    explicit DeterministicNoise(std::uint32_t seed) : state_(seed) {}

    float next() {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float>(state_ >> 8) / static_cast<float>(1u << 24) * 2.0f - 1.0f;
    }

private:
    std::uint32_t state_;
};

inline void addNoiseBurst(std::vector<float>& signal, std::size_t start, double amplitude, double decayMs,
                          std::uint32_t seed = 1, int sampleRate = kTestSampleRate) {
    DeterministicNoise noise(seed);
    const double decaySamples = decayMs * sampleRate / 1000.0;
    for (std::size_t index = start; index < signal.size(); ++index) {
        const double envelope = amplitude * std::exp(-static_cast<double>(index - start) / decaySamples);
        if (envelope < 1e-5) {
            break;
        }
        signal[index] += static_cast<float>(envelope * noise.next());
    }
}

inline void addTone(std::vector<float>& signal, std::size_t start, std::size_t length, double frequency,
                    double amplitude, int sampleRate = kTestSampleRate) {
    const std::size_t ramp = samplesAt(0.002, sampleRate);
    for (std::size_t offset = 0; offset < length && start + offset < signal.size(); ++offset) {
        const auto rampSamples = static_cast<double>(ramp);
        const double attack = offset < ramp ? static_cast<double>(offset) / rampSamples : 1.0;
        const double release = length - offset < ramp ? static_cast<double>(length - offset) / rampSamples : 1.0;
        signal[start + offset] += static_cast<float>(amplitude * attack * release *
                                                     std::sin(2.0 * std::numbers::pi * frequency *
                                                              static_cast<double>(offset) / sampleRate));
    }
}

inline void addLegatoPhrase(std::vector<float>& signal, std::size_t start, std::size_t noteLength,
                            const std::vector<double>& frequencies, double amplitude, int sampleRate = kTestSampleRate) {
    const std::size_t attack = samplesAt(0.002, sampleRate);
    const std::size_t fade = samplesAt(0.05, sampleRate);
    const std::size_t total = noteLength * frequencies.size();
    double phase = 0.0;
    for (std::size_t offset = 0; offset < total && start + offset < signal.size(); ++offset) {
        const double frequency = frequencies[offset / noteLength];
        double envelope = 1.0;
        if (offset < attack) {
            envelope = static_cast<double>(offset) / static_cast<double>(attack);
        } else if (total - offset < fade) {
            envelope = static_cast<double>(total - offset) / static_cast<double>(fade);
        }
        signal[start + offset] += static_cast<float>(amplitude * envelope * std::sin(phase));
        phase += 2.0 * std::numbers::pi * frequency / sampleRate;
    }
}

inline void addBackgroundNoise(std::vector<float>& signal, double amplitude, std::uint32_t seed = 99) {
    DeterministicNoise noise(seed);
    for (auto& sample : signal) {
        sample += static_cast<float>(amplitude * noise.next());
    }
}

inline void writePcm16Wav(const std::filesystem::path& path, const std::vector<float>& interleaved, int channels,
                          int sampleRate = kTestSampleRate) {
    auto writeLittleEndian = [](std::ofstream& out, std::uint32_t value, int bytes) {
        for (int index = 0; index < bytes; ++index) {
            out.put(static_cast<char>((value >> (8 * index)) & 0xFF));
        }
    };
    const auto dataBytes = static_cast<std::uint32_t>(interleaved.size() * 2);
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    writeLittleEndian(out, 36 + dataBytes, 4);
    out.write("WAVEfmt ", 8);
    writeLittleEndian(out, 16, 4);
    writeLittleEndian(out, 1, 2);
    writeLittleEndian(out, static_cast<std::uint32_t>(channels), 2);
    writeLittleEndian(out, static_cast<std::uint32_t>(sampleRate), 4);
    writeLittleEndian(out, static_cast<std::uint32_t>(sampleRate * channels * 2), 4);
    writeLittleEndian(out, static_cast<std::uint32_t>(channels * 2), 2);
    writeLittleEndian(out, 16, 2);
    out.write("data", 4);
    writeLittleEndian(out, dataBytes, 4);
    for (const float sample : interleaved) {
        const auto value = static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f));
        writeLittleEndian(out, static_cast<std::uint16_t>(value), 2);
    }
}

}  // namespace ibs::testing
