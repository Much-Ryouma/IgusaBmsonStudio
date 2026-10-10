#pragma once

#include <vector>

#include "ibs/core/bmson/BmsonDocument.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::core::bmson {

struct ImportResult {
    Project project;
    std::vector<Issue> notices;
};

[[nodiscard]] ImportResult importDocument(const Document& document);

}  // namespace ibs::core::bmson
