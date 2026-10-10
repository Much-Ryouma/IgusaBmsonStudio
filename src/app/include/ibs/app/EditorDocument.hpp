#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ibs/core/model/Project.hpp"

namespace ibs::app {

class EditorDocument {
public:
    using Listener = std::function<void()>;

    explicit EditorDocument(core::Project project = {}, std::optional<std::filesystem::path> path = std::nullopt,
                            std::size_t undoLimit = 200);

    [[nodiscard]] const core::Project& project() const noexcept { return project_; }
    [[nodiscard]] const std::optional<std::filesystem::path>& path() const noexcept { return path_; }
    [[nodiscard]] std::filesystem::path chartDirectory() const;
    [[nodiscard]] bool isDirty() const noexcept { return revision_ != savedRevision_; }

    template <typename Edit>
    decltype(auto) apply(const std::string& label, Edit&& edit) {
        core::Project edited = project_;
        if constexpr (std::is_void_v<std::invoke_result_t<Edit, core::Project&>>) {
            std::forward<Edit>(edit)(edited);
            commit(label, std::move(edited));
        } else {
            auto result = std::forward<Edit>(edit)(edited);
            commit(label, std::move(edited));
            return result;
        }
    }

    [[nodiscard]] bool canUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::string undoLabel() const;
    [[nodiscard]] std::string redoLabel() const;
    void undo();
    void redo();

    void replace(core::Project project, std::optional<std::filesystem::path> path);
    void markSaved(const std::filesystem::path& path);

    std::size_t addListener(Listener listener);
    void removeListener(std::size_t id);

private:
    struct Snapshot {
        std::string label;
        core::Project project;
        std::size_t revision = 0;
    };

    void commit(const std::string& label, core::Project edited);
    void notify() const;

    core::Project project_;
    std::optional<std::filesystem::path> path_;
    std::size_t undoLimit_;
    std::size_t revision_ = 0;
    std::size_t savedRevision_ = 0;
    std::size_t nextRevision_ = 1;
    std::deque<Snapshot> undo_;
    std::deque<Snapshot> redo_;
    std::size_t nextListenerId_ = 1;
    std::vector<std::pair<std::size_t, Listener>> listeners_;
};

}  // namespace ibs::app
