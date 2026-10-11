#include "ibs/core/playback/BmsonRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <utility>

#include "ibs/core/timing/TimingMap.hpp"

namespace ibs::core {

namespace {

constexpr int kOutputChannels = 2;
constexpr int kCoarseRate = 1000;
constexpr double kEnergyFloor = 1e-12;
constexpr double kPositionTolerance = 1e-6;
constexpr double kFineExcerptSeconds = 8.0;

float sourceSample(const AudioBuffer& audio, std::size_t frame, int outputChannel) {
    const auto channels = static_cast<std::size_t>(audio.channels);
    const std::size_t channel = audio.channels == 1 ? 0 : static_cast<std::size_t>(std::min(outputChannel, audio.channels - 1));
    return audio.interleavedSamples[frame * channels + channel];
}

struct Slice {
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    double sourceSeconds = 0.0;
    int triggers = 1;
};

// Why not 鍵盤の数だけ鳴らすのをやめて 1 回にまとめる: 実際のプレーヤーは同じ位置の各ノーツがそれぞれ音を鳴らすので、
// 1 つのスライスを複数レーンに置いた譜面は音が重なる。null テストではそれを差として見つけたいため、そのとおりに鳴らす
std::vector<Slice> slicesOf(const bmson::SoundChannel& channel, const TimingMap& timing, const NoteFilter& include) {
    std::vector<bmson::Note> notes = channel.notes;
    std::stable_sort(notes.begin(), notes.end(), [](const bmson::Note& a, const bmson::Note& b) { return a.y < b.y; });
    std::vector<Slice> slices;
    double anchor = 0.0;
    for (std::size_t first = 0; first < notes.size();) {
        std::size_t last = first;
        while (last < notes.size() && notes[last].y == notes[first].y) {
            ++last;
        }
        bool restart = false;
        bool hasLaneNote = false;
        for (std::size_t index = first; index < last; ++index) {
            restart = restart || !notes[index].c;
            hasLaneNote = hasLaneNote || notes[index].x != 0;
        }
        // Why not 絞り込んだノーツだけで BGM の取り消しを判断する: 取り消し（同じ位置に鍵盤のノーツがあると BGM は鳴らない）は
        // 譜面全体で決まるので、譜面だけ・BGM だけを鳴らすときも、全体で鳴らない音は鳴らさない
        int triggers = 0;
        for (std::size_t index = first; index < last; ++index) {
            const bool sounds = hasLaneNote ? notes[index].x != 0 : true;
            if (sounds && (!include || include(notes[index]))) {
                ++triggers;
            }
        }
        const double start = timing.secondsAt(notes[first].y);
        if (restart) {
            anchor = start;
        }
        Slice slice;
        slice.startSeconds = start;
        slice.endSeconds = last < notes.size() ? timing.secondsAt(notes[last].y) : -1.0;
        slice.sourceSeconds = start - anchor;
        slice.triggers = triggers;
        slices.push_back(slice);
        first = last;
    }
    return slices;
}

void mixSlice(const AudioBuffer& audio, const Slice& slice, AudioBuffer& output) {
    const int rate = output.sampleRate;
    const auto outputFrames = static_cast<std::int64_t>(output.frameCount());
    const auto first = static_cast<std::int64_t>(std::llround(slice.startSeconds * rate));
    const double sourceFrames = static_cast<double>(audio.frameCount());
    const double ratio = static_cast<double>(audio.sampleRate) / rate;
    const double sourceStart = slice.sourceSeconds * audio.sampleRate;
    std::int64_t last = outputFrames;
    if (slice.endSeconds >= 0.0) {
        last = std::min(last, static_cast<std::int64_t>(std::llround(slice.endSeconds * rate)));
    }
    for (std::int64_t frame = std::max<std::int64_t>(0, first); frame < last; ++frame) {
        const double position = sourceStart + static_cast<double>(frame - first) * ratio;
        // Why not 端をちょうど比べる: 秒から位置を求める計算の丸めで、最後のサンプルを鳴らすかどうかが描き方ごとに変わるため、わずかな誤差は許す
        if (position > sourceFrames - 1.0 + kPositionTolerance) {
            break;
        }
        const auto index = std::min(static_cast<std::size_t>(position), audio.frameCount() - 1);
        const std::size_t next = std::min(index + 1, audio.frameCount() - 1);
        const auto fraction = static_cast<float>(position - static_cast<double>(index));
        for (int channel = 0; channel < kOutputChannels; ++channel) {
            const float a = sourceSample(audio, index, channel);
            const float b = sourceSample(audio, next, channel);
            output.interleavedSamples[static_cast<std::size_t>(frame) * kOutputChannels + static_cast<std::size_t>(channel)] +=
                static_cast<float>(slice.triggers) * (a + (b - a) * fraction);
        }
    }
}

std::vector<float> monoOf(const AudioBuffer& buffer) {
    return mixToMono(buffer);
}

std::vector<double> decimate(const std::vector<float>& samples, int fromRate) {
    const auto factor = static_cast<std::size_t>(std::max(1, fromRate / kCoarseRate));
    std::vector<double> result((samples.size() + factor - 1) / factor, 0.0);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        result[index / factor] += samples[index];
    }
    return result;
}

double correlationAt(const std::vector<float>& reference, std::size_t from, std::size_t to,
                     const std::vector<float>& candidate, std::int64_t lag) {
    const auto candidateSize = static_cast<std::int64_t>(candidate.size());
    const std::int64_t first = std::max<std::int64_t>(static_cast<std::int64_t>(from), -lag);
    const std::int64_t last = std::min(static_cast<std::int64_t>(to), candidateSize - lag);
    if (first >= last) {
        return 0.0;
    }
    const float* a = reference.data() + first;
    const float* b = candidate.data() + first + lag;
    const auto count = static_cast<std::size_t>(last - first);
    double partial[4] = {0.0, 0.0, 0.0, 0.0};
    std::size_t index = 0;
    for (; index + 4 <= count; index += 4) {
        partial[0] += static_cast<double>(a[index]) * b[index];
        partial[1] += static_cast<double>(a[index + 1]) * b[index + 1];
        partial[2] += static_cast<double>(a[index + 2]) * b[index + 2];
        partial[3] += static_cast<double>(a[index + 3]) * b[index + 3];
    }
    for (; index < count; ++index) {
        partial[0] += static_cast<double>(a[index]) * b[index];
    }
    return (partial[0] + partial[1]) + (partial[2] + partial[3]);
}

void fft(std::vector<std::complex<double>>& values, bool inverse) {
    const std::size_t size = values.size();
    for (std::size_t index = 1, reversed = 0; index < size; ++index) {
        std::size_t bit = size >> 1;
        for (; (reversed & bit) != 0; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(values[index], values[reversed]);
        }
    }
    for (std::size_t length = 2; length <= size; length <<= 1) {
        const double angle = 2.0 * std::numbers::pi / static_cast<double>(length) * (inverse ? 1.0 : -1.0);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (std::size_t block = 0; block < size; block += length) {
            std::complex<double> twiddle(1.0, 0.0);
            for (std::size_t offset = 0; offset < length / 2; ++offset) {
                const auto even = values[block + offset];
                const auto odd = values[block + offset + length / 2] * twiddle;
                values[block + offset] = even + odd;
                values[block + offset + length / 2] = even - odd;
                twiddle *= step;
            }
        }
    }
    if (inverse) {
        for (auto& value : values) {
            value /= static_cast<double>(size);
        }
    }
}

// Why not ずれ候補ごとに内積を取る: 曲全体×数千通りのずれで実曲が数秒かかったため、FFT で全候補を一度に求める
std::int64_t bestLagByFft(const std::vector<double>& reference, const std::vector<double>& candidate, std::int64_t maxLag) {
    std::size_t size = 1;
    while (size < reference.size() + candidate.size()) {
        size <<= 1;
    }
    std::vector<std::complex<double>> a(size);
    std::vector<std::complex<double>> b(size);
    std::copy(reference.begin(), reference.end(), a.begin());
    std::copy(candidate.begin(), candidate.end(), b.begin());
    fft(a, false);
    fft(b, false);
    for (std::size_t index = 0; index < size; ++index) {
        a[index] = std::conj(a[index]) * b[index];
    }
    fft(a, true);
    std::int64_t best = 0;
    double bestValue = -1.0e300;
    const auto signedSize = static_cast<std::int64_t>(size);
    for (std::int64_t lag = -maxLag; lag <= maxLag; ++lag) {
        if (lag <= -signedSize || lag >= signedSize) {
            continue;
        }
        const double value = a[static_cast<std::size_t>(lag < 0 ? lag + signedSize : lag)].real();
        if (value > bestValue + 1e-9 * std::abs(bestValue)) {
            bestValue = value;
            best = lag;
        }
    }
    return best;
}

std::size_t loudestExcerptStart(const std::vector<float>& samples, std::size_t length) {
    if (samples.size() <= length) {
        return 0;
    }
    double energy = 0.0;
    for (std::size_t index = 0; index < length; ++index) {
        energy += static_cast<double>(samples[index]) * samples[index];
    }
    double best = energy;
    std::size_t bestStart = 0;
    for (std::size_t start = 1; start + length <= samples.size(); ++start) {
        energy += static_cast<double>(samples[start + length - 1]) * samples[start + length - 1] -
                  static_cast<double>(samples[start - 1]) * samples[start - 1];
        if (energy > best) {
            best = energy;
            bestStart = start;
        }
    }
    return bestStart;
}

double toDb(double ratio) { return 10.0 * std::log10(std::max(ratio, kEnergyFloor)); }

}  // namespace

AudioBuffer renderBmson(const bmson::Document& document, const std::map<std::string, const AudioBuffer*>& audioByChannel,
                        int outputSampleRate, const NoteFilter& include) {
    if (outputSampleRate <= 0) {
        throw std::invalid_argument("outputSampleRate must be positive");
    }
    const TimingMap timing = document.timing.toTimingMap();
    double endSeconds = 0.0;
    std::vector<std::pair<const AudioBuffer*, std::vector<Slice>>> channels;
    for (const auto& channel : document.soundChannels) {
        const auto found = audioByChannel.find(channel.name);
        if (found == audioByChannel.end() || found->second == nullptr || found->second->sampleRate <= 0) {
            continue;
        }
        auto slices = slicesOf(channel, timing, include);
        for (const auto& slice : slices) {
            if (slice.triggers == 0) {
                continue;
            }
            const double audioLeft = found->second->durationSeconds() - slice.sourceSeconds;
            const double end = slice.endSeconds >= 0.0 ? std::min(slice.endSeconds, slice.startSeconds + audioLeft)
                                                       : slice.startSeconds + audioLeft;
            endSeconds = std::max(endSeconds, end);
        }
        channels.emplace_back(found->second, std::move(slices));
    }
    AudioBuffer output;
    output.sampleRate = outputSampleRate;
    output.channels = kOutputChannels;
    output.interleavedSamples.assign(static_cast<std::size_t>(std::ceil(endSeconds * outputSampleRate)) * kOutputChannels, 0.0f);
    for (const auto& [audio, slices] : channels) {
        for (const auto& slice : slices) {
            if (slice.triggers > 0) {
                mixSlice(*audio, slice, output);
            }
        }
    }
    return output;
}

// Why not 先頭をそろえてそのまま引く: プレーヤーの書き出しは曲頭に無音を足したり音量を変えたりするため、
// まず粗い相関でずれを探してから細かく合わせ、音量の倍率をそろえてから差を取る
RenderComparison compareRenders(const AudioBuffer& reference, const AudioBuffer& candidate,
                                const RenderComparisonOptions& options) {
    if (reference.sampleRate <= 0 || reference.sampleRate != candidate.sampleRate) {
        throw std::invalid_argument("both renders need the same positive sample rate");
    }
    const int rate = reference.sampleRate;
    const auto ref = monoOf(reference);
    const auto cand = monoOf(candidate);

    const auto coarseRef = decimate(ref, rate);
    const auto coarseCand = decimate(cand, rate);
    const auto factor = static_cast<std::int64_t>(std::max(1, rate / kCoarseRate));
    const auto maxCoarseLag = static_cast<std::int64_t>(options.maxOffsetMs * kCoarseRate / 1000.0);
    const std::int64_t bestCoarse = bestLagByFft(coarseRef, coarseCand, maxCoarseLag);

    // Why not 細かい合わせも曲全体で取る: 粗いずれの前後だけを確かめれば足り、最も音の大きい区間で十分に決まる
    const auto excerpt = static_cast<std::size_t>(kFineExcerptSeconds * rate);
    const auto from = loudestExcerptStart(ref, excerpt);
    const auto to = std::min(ref.size(), from + excerpt);
    std::int64_t bestLag = bestCoarse * factor;
    double bestValue = -1.0e300;
    for (std::int64_t lag = bestCoarse * factor - factor; lag <= bestCoarse * factor + factor; ++lag) {
        const double value = correlationAt(ref, from, to, cand, lag);
        if (value > bestValue) {
            bestValue = value;
            bestLag = lag;
        }
    }

    RenderComparison result;
    result.offsetSeconds = static_cast<double>(bestLag) / rate;
    const auto at = [&](std::size_t index) {
        const auto other = static_cast<std::int64_t>(index) + bestLag;
        return other >= 0 && other < static_cast<std::int64_t>(cand.size()) ? static_cast<double>(cand[static_cast<std::size_t>(other)]) : 0.0;
    };
    const auto window = static_cast<std::size_t>(std::max(1.0, options.windowSeconds * rate));
    const double floor = std::pow(10.0, options.silenceFloorDb / 10.0) * static_cast<double>(window);
    // Why not 全体の最小二乗で音量の倍率を決める: 一部の区間だけ音が二重になっていると倍率が全体に引きずられ、
    // 正しい区間まで差があるように見えるため、区間ごとの倍率の中央値を使う
    std::vector<double> ratios;
    for (std::size_t start = 0; start < ref.size(); start += window) {
        double referenceEnergy = 0.0;
        double candidateEnergy = 0.0;
        for (std::size_t index = start; index < std::min(ref.size(), start + window); ++index) {
            referenceEnergy += static_cast<double>(ref[index]) * ref[index];
            candidateEnergy += at(index) * at(index);
        }
        if (referenceEnergy > floor && candidateEnergy > kEnergyFloor) {
            ratios.push_back(std::sqrt(referenceEnergy / candidateEnergy));
        }
    }
    if (!ratios.empty()) {
        std::nth_element(ratios.begin(), ratios.begin() + static_cast<std::ptrdiff_t>(ratios.size() / 2), ratios.end());
        result.gain = ratios[ratios.size() / 2];
    }

    double totalReference = 0.0;
    double totalDifference = 0.0;
    for (std::size_t start = 0; start < ref.size(); start += window) {
        double referenceEnergy = 0.0;
        double differenceEnergy = 0.0;
        for (std::size_t index = start; index < std::min(ref.size(), start + window); ++index) {
            const double difference = result.gain * at(index) - ref[index];
            referenceEnergy += static_cast<double>(ref[index]) * ref[index];
            differenceEnergy += difference * difference;
        }
        totalReference += referenceEnergy;
        totalDifference += differenceEnergy;
        result.windows.push_back(ResidualWindow{static_cast<double>(start) / rate,
                                                toDb(differenceEnergy / std::max(referenceEnergy, floor))});
    }
    result.residualDb = toDb(totalDifference / std::max(totalReference, kEnergyFloor));
    return result;
}

}  // namespace ibs::core
