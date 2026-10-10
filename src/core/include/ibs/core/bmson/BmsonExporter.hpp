#pragma once

#include "ibs/core/bmson/BmsonDocument.hpp"
#include "ibs/core/model/Project.hpp"

namespace ibs::core::bmson {

[[nodiscard]] Document exportDocument(const Project& project);

}  // namespace ibs::core::bmson
