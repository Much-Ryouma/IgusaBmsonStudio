#pragma once

#include <vector>

#include "ibs/core/mode/KeyMode.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/ports/FileSystemPort.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::core {

struct ValidationContext {
    const KeyMode* keyMode = nullptr;
    const FileSystemPort* fileSystem = nullptr;
};

[[nodiscard]] std::vector<Issue> validateProject(const Project& project, const ValidationContext& context);

[[nodiscard]] std::size_t countPlayableNotes(const Project& project, const KeyMode& keyMode);

}  // namespace ibs::core
