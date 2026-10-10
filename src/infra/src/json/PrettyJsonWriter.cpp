#include "ibs/infra/json/PrettyJsonWriter.hpp"

#include <algorithm>

namespace ibs::infra {

namespace {

using Json = nlohmann::ordered_json;

bool isLeafContainer(const Json& value) {
    if (!value.is_structured() || value.empty()) {
        return true;
    }
    return std::none_of(value.begin(), value.end(), [](const Json& child) { return child.is_structured(); });
}

std::string compact(const Json& value) {
    if (!value.is_object() || value.empty()) {
        return value.dump(-1, ' ', false, nlohmann::detail::error_handler_t::strict);
    }
    std::string text = "{";
    bool first = true;
    for (const auto& [key, child] : value.items()) {
        text += first ? "" : ", ";
        text += Json(key).dump() + ": " + child.dump(-1, ' ', false, nlohmann::detail::error_handler_t::strict);
        first = false;
    }
    return text + "}";
}

void write(const Json& value, int depth, int indentWidth, std::string& out) {
    if (isLeafContainer(value)) {
        out += compact(value);
        return;
    }
    const std::string innerIndent(static_cast<std::size_t>((depth + 1) * indentWidth), ' ');
    const std::string closingIndent(static_cast<std::size_t>(depth * indentWidth), ' ');
    const bool isObject = value.is_object();
    out += isObject ? "{\n" : "[\n";
    bool first = true;
    for (const auto& [key, child] : value.items()) {
        out += first ? "" : ",\n";
        out += innerIndent;
        if (isObject) {
            out += Json(key).dump() + ": ";
        }
        write(child, depth + 1, indentWidth, out);
        first = false;
    }
    out += "\n" + closingIndent + (isObject ? "}" : "]");
}

}  // namespace

std::string writeJsonWithFlatLeaves(const nlohmann::ordered_json& value, int indentWidth) {
    std::string out;
    write(value, 0, indentWidth, out);
    out += "\n";
    return out;
}

}  // namespace ibs::infra
