#include "ibs/infra/mode/KeyModeJsonLoader.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::infra {

namespace {

core::LaneKind laneKindFrom(const std::string& text) {
    if (text == "white") { return core::LaneKind::White; }
    if (text == "black") { return core::LaneKind::Black; }
    if (text == "scratch") { return core::LaneKind::Scratch; }
    if (text == "other") { return core::LaneKind::Other; }
    throw std::invalid_argument("unknown lane kind: " + text);
}

}  // namespace

core::KeyMode parseKeyMode(const std::string& jsonText) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(jsonText);
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("mode definition is not valid JSON: ") + error.what());
    }
    try {
        std::vector<core::LaneDefinition> lanes;
        for (const auto& lane : root.at("lanes")) {
            lanes.push_back({lane.at("x").get<int>(), lane.value("label", std::string{}),
                             laneKindFrom(lane.value("kind", std::string("other"))), lane.value("width", 1.0)});
        }
        return core::KeyMode(root.at("mode_hint").get<std::string>(), root.value("display_name", std::string{}),
                             std::move(lanes), root.at("bgm_column_start_x").get<int>());
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("mode definition is incomplete: ") + error.what());
    }
}

core::KeyModeCatalog loadKeyModeCatalog(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    core::KeyModeCatalog catalog;
    for (const auto& file : files) {
        catalog.add(parseKeyMode(readTextFile(file)));
    }
    return catalog;
}

}  // namespace ibs::infra
