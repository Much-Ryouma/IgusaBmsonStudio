#include "ibs/app/RecentFiles.hpp"

#include <algorithm>

#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

namespace {

std::string keyOf(const std::filesystem::path& file) {
    return infra::pathToUtf8(file.lexically_normal().make_preferred());
}

}  // namespace

std::vector<std::string> withoutRecentFile(std::vector<std::string> recent, const std::filesystem::path& file) {
    const auto key = keyOf(file);
    std::erase_if(recent, [&](const std::string& entry) { return keyOf(infra::pathFromUtf8(entry)) == key; });
    return recent;
}

std::vector<std::string> withRecentFile(std::vector<std::string> recent, const std::filesystem::path& file, std::size_t limit) {
    recent = withoutRecentFile(std::move(recent), file);
    recent.insert(recent.begin(), keyOf(file));
    if (recent.size() > limit) {
        recent.resize(limit);
    }
    return recent;
}

}  // namespace ibs::app
