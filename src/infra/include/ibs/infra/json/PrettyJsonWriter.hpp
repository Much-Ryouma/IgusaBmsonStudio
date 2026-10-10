#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace ibs::infra {

[[nodiscard]] std::string writeJsonWithFlatLeaves(const nlohmann::ordered_json& value, int indentWidth = 4);

}  // namespace ibs::infra
