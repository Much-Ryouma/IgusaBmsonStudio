#include "ibs/infra/bmson/BmsonJsonCodec.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>
#include <string_view>

#include <nlohmann/json.hpp>

#include "ibs/core/model/Project.hpp"
#include "ibs/infra/json/PrettyJsonWriter.hpp"

namespace ibs::infra {

namespace {

using Json = nlohmann::json;
using OrderedJson = nlohmann::ordered_json;
using core::Issue;
using core::Pulse;
using core::Severity;
namespace bmson = core::bmson;

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";

const std::set<std::string> kKnownTopLevelKeys = {"version",     "info",           "lines", "bpm_events",
                                                   "stop_events", "sound_channels", "bga"};
const std::set<std::string> kKnownInfoKeys = {
    "title",      "subtitle",    "artist",         "subartists",  "genre",        "mode_hint",
    "chart_name", "level",       "init_bpm",       "judge_rank",  "total",        "back_image",
    "eyecatch_image", "title_image", "banner_image", "preview_music", "resolution"};

class Reader {
public:
    explicit Reader(std::vector<Issue>& notices) : notices_(notices) {}

    void reportUnknownKeys(const Json& object, const std::set<std::string>& known, const std::string& where) {
        for (const auto& [key, value] : object.items()) {
            if (known.count(key) == 0) {
                notices_.push_back({Severity::Warning, "JSN-UNKNOWN-FIELD",
                                    where + key + " は未対応のフィールドのため、保存すると失われます"});
            }
        }
    }

    static std::string string(const Json& object, const char* key, std::string fallback = {}) {
        const auto found = object.find(key);
        if (found == object.end() || found->is_null()) {
            return fallback;
        }
        if (!found->is_string()) {
            throw BmsonFormatError(std::string(key) + " は文字列である必要があります");
        }
        return found->get<std::string>();
    }

    static double number(const Json& object, const char* key, double fallback) {
        const auto found = object.find(key);
        if (found == object.end() || found->is_null()) {
            return fallback;
        }
        if (!found->is_number()) {
            throw BmsonFormatError(std::string(key) + " は数値である必要があります");
        }
        return found->get<double>();
    }

    static Pulse pulse(const Json& object, const char* key, Pulse fallback, const std::string& where) {
        const double value = number(object, key, static_cast<double>(fallback));
        if (std::floor(value) != value || std::abs(value) > static_cast<double>(std::numeric_limits<Pulse>::max())) {
            throw BmsonFormatError(where + key + " は整数である必要があります");
        }
        return static_cast<Pulse>(value);
    }

    static const Json& arrayOrEmpty(const Json& object, const char* key) {
        static const Json kEmpty = Json::array();
        const auto found = object.find(key);
        if (found == object.end() || found->is_null()) {
            return kEmpty;
        }
        if (!found->is_array()) {
            throw BmsonFormatError(std::string(key) + " は配列である必要があります");
        }
        return *found;
    }

    void warn(const char* code, std::string message) { notices_.push_back({Severity::Warning, code, std::move(message)}); }

private:
    std::vector<Issue>& notices_;
};

void readInfo(const Json& info, bmson::Document& document, Reader& reader) {
    if (!info.is_object()) {
        throw BmsonFormatError("info がありません");
    }
    reader.reportUnknownKeys(info, kKnownInfoKeys, "info.");

    auto& song = document.info;
    song.title = Reader::string(info, "title");
    song.subtitle = Reader::string(info, "subtitle");
    song.artist = Reader::string(info, "artist");
    for (const auto& subartist : Reader::arrayOrEmpty(info, "subartists")) {
        if (!subartist.is_string()) {
            throw BmsonFormatError("info.subartists の要素は文字列である必要があります");
        }
        song.subartists.push_back(subartist.get<std::string>());
    }
    song.genre = Reader::string(info, "genre");
    song.modeHint = Reader::string(info, "mode_hint", "beat-7k");
    song.chartName = Reader::string(info, "chart_name");
    song.backImage = Reader::string(info, "back_image");
    song.eyecatchImage = Reader::string(info, "eyecatch_image");
    song.titleImage = Reader::string(info, "title_image");
    song.bannerImage = Reader::string(info, "banner_image");
    song.previewMusic = Reader::string(info, "preview_music");
    song.judgeRank = Reader::number(info, "judge_rank", 100.0);

    const double level = Reader::number(info, "level", 0.0);
    if (level < 0.0) {
        reader.warn("JSN-NEGATIVE-LEVEL", "info.level が負のため 0 として読み込みます");
    }
    song.level = level < 0.0 ? 0u : static_cast<std::uint32_t>(level);

    const double total = Reader::number(info, "total", 100.0);
    if (total < 0.0) {
        reader.warn("JSN-NEGATIVE-TOTAL", "info.total が負のため、仕様どおり絶対値として読み込みます");
    }
    song.total = std::abs(total);

    if (!info.contains("init_bpm") || info["init_bpm"].is_null()) {
        throw BmsonFormatError("info.init_bpm がありません（bmson 仕様で必須です）");
    }
    document.timing.initialBpm = Reader::number(info, "init_bpm", 0.0);

    const Pulse resolution = Reader::pulse(info, "resolution", 0, "info.");
    if (resolution < 0) {
        reader.warn("JSN-NEGATIVE-RESOLUTION", "info.resolution が負のため、仕様どおり絶対値として読み込みます");
    }
    const Pulse absoluteResolution = resolution < 0 ? -resolution : resolution;
    document.timing.resolution =
        absoluteResolution == 0 ? core::kDefaultResolution : static_cast<int>(absoluteResolution);
}

void readTiming(const Json& root, bmson::Document& document) {
    const auto linesIt = root.find("lines");
    if (linesIt != root.end() && !linesIt->is_null()) {
        std::vector<Pulse> lines;
        for (const auto& line : Reader::arrayOrEmpty(root, "lines")) {
            lines.push_back(Reader::pulse(line, "y", 0, "lines[]."));
        }
        document.timing.barLines = std::move(lines);
    }
    for (const auto& event : Reader::arrayOrEmpty(root, "bpm_events")) {
        document.timing.bpmEvents.push_back(
            {Reader::pulse(event, "y", 0, "bpm_events[]."), Reader::number(event, "bpm", 0.0)});
    }
    for (const auto& event : Reader::arrayOrEmpty(root, "stop_events")) {
        document.timing.stopEvents.push_back(
            {Reader::pulse(event, "y", 0, "stop_events[]."), Reader::pulse(event, "duration", 0, "stop_events[].")});
    }
}

int readLane(const Json& note) {
    const auto found = note.find("x");
    if (found == note.end() || found->is_null()) {
        return core::kBgmLane;
    }
    if (!found->is_number_integer() && !(found->is_number() && std::floor(found->get<double>()) == found->get<double>())) {
        throw BmsonFormatError("sound_channels[].notes[].x は整数か null である必要があります");
    }
    return static_cast<int>(found->get<double>());
}

void readSoundChannels(const Json& root, bmson::Document& document) {
    const auto found = root.find("sound_channels");
    if (found == root.end() || !found->is_array()) {
        throw BmsonFormatError("sound_channels がありません");
    }
    for (const auto& channelJson : *found) {
        bmson::SoundChannel channel;
        channel.name = Reader::string(channelJson, "name");
        for (const auto& noteJson : Reader::arrayOrEmpty(channelJson, "notes")) {
            if (!noteJson.contains("y")) {
                throw BmsonFormatError("sound_channels '" + channel.name + "' に y のないノーツがあります");
            }
            bmson::Note note;
            note.x = readLane(noteJson);
            note.y = Reader::pulse(noteJson, "y", 0, "notes[].");
            note.l = Reader::pulse(noteJson, "l", 0, "notes[].");
            const auto c = noteJson.find("c");
            note.c = c != noteJson.end() && c->is_boolean() && c->get<bool>();
            channel.notes.push_back(note);
        }
        document.soundChannels.push_back(std::move(channel));
    }
}

std::vector<core::BgaEvent> readBgaEvents(const Json& bga, const char* key) {
    std::vector<core::BgaEvent> events;
    for (const auto& event : Reader::arrayOrEmpty(bga, key)) {
        events.push_back({Reader::pulse(event, "y", 0, std::string(key) + "[]."),
                          static_cast<std::uint32_t>(Reader::pulse(event, "id", 0, std::string(key) + "[]."))});
    }
    return events;
}

void readBga(const Json& root, bmson::Document& document) {
    const auto found = root.find("bga");
    if (found == root.end() || found->is_null()) {
        return;
    }
    for (const auto& header : Reader::arrayOrEmpty(*found, "bga_header")) {
        document.bga.headers.push_back({static_cast<std::uint32_t>(Reader::pulse(header, "id", 0, "bga_header[].")),
                                        Reader::string(header, "name")});
    }
    document.bga.bgaEvents = readBgaEvents(*found, "bga_events");
    document.bga.layerEvents = readBgaEvents(*found, "layer_events");
    document.bga.poorEvents = readBgaEvents(*found, "poor_events");
}

OrderedJson numberValue(double value) {
    // Why not 常に double で出力: 100 が 100.0 と書かれると、既存ツールで作ったファイルとの差分が見づらくなるため
    if (std::floor(value) == value && std::abs(value) < 1e15) {
        return static_cast<std::int64_t>(value);
    }
    return value;
}

OrderedJson writeBgaEvents(const std::vector<core::BgaEvent>& events) {
    OrderedJson array = OrderedJson::array();
    for (const auto& event : events) {
        array.push_back({{"y", event.y}, {"id", event.id}});
    }
    return array;
}

}  // namespace

BmsonParseResult parseBmson(const std::string& text) {
    std::string_view body = text;
    if (body.substr(0, kUtf8Bom.size()) == kUtf8Bom) {
        body.remove_prefix(kUtf8Bom.size());
    }

    Json root;
    try {
        root = Json::parse(body);
    } catch (const Json::parse_error& error) {
        throw BmsonFormatError(std::string("JSON として読み込めません: ") + error.what());
    }
    if (!root.is_object()) {
        throw BmsonFormatError("bmson の最上位はオブジェクトである必要があります");
    }

    BmsonParseResult result;
    Reader reader(result.notices);
    reader.reportUnknownKeys(root, kKnownTopLevelKeys, "");

    const auto version = root.find("version");
    if (version == root.end() || version->is_null()) {
        throw BmsonFormatError("version がありません。bmson 1.0.0 より前の形式には対応していません");
    }
    if (!version->is_string() || version->get<std::string>().rfind("1.", 0) != 0) {
        throw BmsonFormatError("対応していない bmson のバージョンです: " + version->dump());
    }
    result.document.version = version->get<std::string>();

    readInfo(root.contains("info") ? root["info"] : Json(), result.document, reader);
    readTiming(root, result.document);
    readSoundChannels(root, result.document);
    readBga(root, result.document);
    return result;
}

std::string serializeBmson(const core::bmson::Document& document) {
    const auto& song = document.info;
    const auto& timing = document.timing;

    OrderedJson info;
    info["title"] = song.title;
    info["subtitle"] = song.subtitle;
    info["artist"] = song.artist;
    info["subartists"] = song.subartists;
    info["genre"] = song.genre;
    info["mode_hint"] = song.modeHint;
    info["chart_name"] = song.chartName;
    info["level"] = song.level;
    info["init_bpm"] = numberValue(timing.initialBpm);
    info["judge_rank"] = numberValue(song.judgeRank);
    info["total"] = numberValue(song.total);
    info["back_image"] = song.backImage;
    info["eyecatch_image"] = song.eyecatchImage;
    info["title_image"] = song.titleImage;
    info["banner_image"] = song.bannerImage;
    info["preview_music"] = song.previewMusic;
    info["resolution"] = timing.resolution;

    OrderedJson root;
    root["version"] = document.version;
    root["info"] = std::move(info);
    if (timing.barLines.has_value()) {
        OrderedJson lines = OrderedJson::array();
        for (const Pulse y : *timing.barLines) {
            lines.push_back({{"y", y}});
        }
        root["lines"] = std::move(lines);
    }

    OrderedJson bpmEvents = OrderedJson::array();
    for (const auto& event : timing.bpmEvents) {
        bpmEvents.push_back({{"y", event.y}, {"bpm", numberValue(event.bpm)}});
    }
    root["bpm_events"] = std::move(bpmEvents);

    OrderedJson stopEvents = OrderedJson::array();
    for (const auto& event : timing.stopEvents) {
        stopEvents.push_back({{"y", event.y}, {"duration", event.durationPulses}});
    }
    root["stop_events"] = std::move(stopEvents);

    OrderedJson channels = OrderedJson::array();
    for (const auto& channel : document.soundChannels) {
        OrderedJson notes = OrderedJson::array();
        for (const auto& note : channel.notes) {
            notes.push_back({{"x", note.x}, {"y", note.y}, {"l", note.l}, {"c", note.c}});
        }
        channels.push_back({{"name", channel.name}, {"notes", std::move(notes)}});
    }
    root["sound_channels"] = std::move(channels);

    OrderedJson headers = OrderedJson::array();
    for (const auto& header : document.bga.headers) {
        headers.push_back({{"id", header.id}, {"name", header.name}});
    }
    OrderedJson bga;
    bga["bga_header"] = std::move(headers);
    bga["bga_events"] = writeBgaEvents(document.bga.bgaEvents);
    bga["layer_events"] = writeBgaEvents(document.bga.layerEvents);
    bga["poor_events"] = writeBgaEvents(document.bga.poorEvents);
    root["bga"] = std::move(bga);

    return writeJsonWithFlatLeaves(root);
}

}  // namespace ibs::infra
