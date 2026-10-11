#pragma once

#include <optional>

#include <vector>

#include "ibs/core/mode/KeyMode.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/ports/FileSystemPort.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::core {

struct ImageSize {
    int width = 0;
    int height = 0;
};

struct ValidationContext {
    const KeyMode* keyMode = nullptr;
    const FileSystemPort* fileSystem = nullptr;
    std::optional<ImageSize> bannerImageSize;
};

[[nodiscard]] std::vector<Issue> validateProject(const Project& project, const ValidationContext& context);

[[nodiscard]] std::size_t countPlayableNotes(const Project& project, const KeyMode& keyMode);

}  // namespace ibs::core
