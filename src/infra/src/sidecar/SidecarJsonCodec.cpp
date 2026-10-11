#include "ibs/infra/sidecar/SidecarJsonCodec.hpp"

#include <array>
#include <utility>

#include <nlohmann/json.hpp>

#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/json/PrettyJsonWriter.hpp"

namespace ibs::infra {

namespace {

using Json = nlohmann::json;
using OrderedJson = nlohmann::ordered_json;

constexpr const char* kFormatName = "igusa-bmson-studio-sidecar";

constexpr std::array<std::pair<core::CutOrigin, const char*>, 3> kOrigins{{
    {core::CutOrigin::Imported, "imported"},
    {core::CutOrigin::Auto, "auto"},
    {core::CutOrigin::Manual, "manual"},
}};

constexpr std::array<std::pair<core::OnsetFunction, const char*>, 3> kFunctions{{
    {core::OnsetFunction::SpectralFlux, "flux"},
    {core::OnsetFunction::Energy, "energy"},
    {core::OnsetFunction::HighFrequencyContent, "hfc"},
}};

template <typename Enum, std::size_t Size>
const char* nameOf(const std::array<std::pair<Enum, const char*>, Size>& table, Enum value) {
    for (const auto& [candidate, name] : table) {
        if (candidate == value) {
            return name;
        }
    }
    return table.front().second;
}

template <typename Enum, std::size_t Size>
Enum valueOf(const std::array<std::pair<Enum, const char*>, Size>& table, const std::string& name, const char* field) {
    for (const auto& [value, candidate] : table) {
        if (name == candidate) {
            return value;
        }
    }
    throw SidecarFormatError(std::string(field) + " の値 '" + name + "' は未対応です");
}

OrderedJson writeSlice(const core::SliceSettings& slice) {
    OrderedJson onset;
    onset["function"] = nameOf(kFunctions, slice.onset.function);
    onset["sensitivity"] = slice.onset.sensitivity;
    onset["min_interval_ms"] = slice.onset.minimumIntervalMs;
    onset["silence_gate_db"] = slice.onset.silenceGateDb;
    onset["preroll_ms"] = slice.onset.prerollMs;
    onset["frame_size"] = slice.onset.frameSize;
    onset["hop_size"] = slice.onset.hopSize;
    OrderedJson quantize;
    quantize["grid"] = slice.quantize.divisionsPerWholeNote;
    quantize["strength"] = slice.quantize.strength;
    quantize["window_ms"] = slice.quantize.windowMs ? OrderedJson(*slice.quantize.windowMs) : OrderedJson(nullptr);
    quantize["attack_clip_warning_ms"] = slice.quantize.attackClipWarningMs;
    OrderedJson result;
    result["onset"] = std::move(onset);
    result["quantize"] = std::move(quantize);
    return result;
}

core::SliceSettings readSlice(const Json& json) {
    core::SliceSettings slice;
    const Json& onset = json.at("onset");
    slice.onset.function = valueOf(kFunctions, onset.value("function", std::string("flux")), "onset.function");
    slice.onset.sensitivity = onset.value("sensitivity", slice.onset.sensitivity);
    slice.onset.minimumIntervalMs = onset.value("min_interval_ms", slice.onset.minimumIntervalMs);
    slice.onset.silenceGateDb = onset.value("silence_gate_db", slice.onset.silenceGateDb);
    slice.onset.prerollMs = onset.value("preroll_ms", slice.onset.prerollMs);
    slice.onset.frameSize = onset.value("frame_size", slice.onset.frameSize);
    slice.onset.hopSize = onset.value("hop_size", slice.onset.hopSize);
    const Json& quantize = json.at("quantize");
    slice.quantize.divisionsPerWholeNote = quantize.value("grid", slice.quantize.divisionsPerWholeNote);
    slice.quantize.strength = quantize.value("strength", slice.quantize.strength);
    if (const auto window = quantize.find("window_ms"); window != quantize.end() && window->is_number()) {
        slice.quantize.windowMs = window->get<double>();
    }
    slice.quantize.attackClipWarningMs = quantize.value("attack_clip_warning_ms", slice.quantize.attackClipWarningMs);
    return slice;
}

}  // namespace

std::string serializeSidecar(const core::SidecarData& data) {
    OrderedJson root;
    root["format"] = kFormatName;
    root["version"] = data.version;
    OrderedJson placements = OrderedJson::array();
    for (const auto& placement : data.placements) {
        OrderedJson entry;
        entry["asset"] = placement.asset;
        entry["y"] = placement.startPulse;
        entry["tempo_affected"] = placement.tempoAffected;
        OrderedJson cuts = OrderedJson::array();
        for (const auto& cut : placement.cuts) {
            OrderedJson item;
            item["offset"] = cut.offsetPulses;
            item["origin"] = nameOf(kOrigins, cut.origin);
            cuts.push_back(std::move(item));
        }
        entry["cuts"] = std::move(cuts);
        if (placement.slice) {
            entry["slice"] = writeSlice(*placement.slice);
        }
        placements.push_back(std::move(entry));
    }
    root["placements"] = std::move(placements);
    return writeJsonWithFlatLeaves(root);
}

core::SidecarData parseSidecar(const std::string& text) {
    try {
        const Json root = Json::parse(text);
        if (root.value("format", std::string()) != kFormatName) {
            throw SidecarFormatError("IgusaBmsonStudio のサイドカーファイルではありません");
        }
        core::SidecarData data;
        data.version = root.value("version", 0);
        if (data.version > core::kSidecarVersion) {
            throw SidecarFormatError("新しい版のサイドカーファイルです（version " + std::to_string(data.version) + "）");
        }
        for (const auto& entry : root.value("placements", Json::array())) {
            core::SidecarPlacement placement;
            placement.asset = entry.at("asset").get<std::string>();
            placement.startPulse = entry.at("y").get<core::Pulse>();
            placement.tempoAffected = entry.value("tempo_affected", false);
            for (const auto& cut : entry.value("cuts", Json::array())) {
                placement.cuts.push_back(core::SidecarCut{cut.at("offset").get<core::Pulse>(),
                                                          valueOf(kOrigins, cut.at("origin").get<std::string>(), "cuts[].origin")});
            }
            if (const auto slice = entry.find("slice"); slice != entry.end()) {
                placement.slice = readSlice(*slice);
            }
            data.placements.push_back(std::move(placement));
        }
        return data;
    } catch (const nlohmann::json::exception& error) {
        throw SidecarFormatError(std::string("サイドカーファイルを読めません: ") + error.what());
    }
}

// Why not <name>.bmson.ibs.json にする: 仕様書の決定（<name>.ibs.json）に合わせ、エクスプローラーで bmson と
// 並べたときに名前で対応が分かるようにする
std::filesystem::path sidecarPathFor(const std::filesystem::path& bmsonPath) {
    auto path = bmsonPath;
    path.replace_extension(".ibs.json");
    return path;
}

std::optional<core::SidecarData> loadSidecarFile(const std::filesystem::path& sidecarPath) {
    std::error_code error;
    if (!std::filesystem::exists(sidecarPath, error)) {
        return std::nullopt;
    }
    return parseSidecar(readTextFile(sidecarPath));
}

void saveSidecarFile(const std::filesystem::path& sidecarPath, const core::SidecarData& data) {
    writeTextFileAtomically(sidecarPath, serializeSidecar(data));
}

}  // namespace ibs::infra
