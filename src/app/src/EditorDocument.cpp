#include "ibs/app/EditorDocument.hpp"

#include <algorithm>

namespace ibs::app {

EditorDocument::EditorDocument(core::Project project, std::optional<std::filesystem::path> path, std::size_t undoLimit)
    : project_(std::move(project)), path_(std::move(path)), undoLimit_(std::max<std::size_t>(1, undoLimit)) {}

std::filesystem::path EditorDocument::chartDirectory() const {
    return path_ ? path_->parent_path() : std::filesystem::path{};
}

std::string EditorDocument::undoLabel() const { return undo_.empty() ? std::string{} : undo_.back().label; }

std::string EditorDocument::redoLabel() const { return redo_.empty() ? std::string{} : redo_.back().label; }

// Why not 操作ごとに逆操作（コマンド）を書く: 配置の削除のように切断点・ノーツへ波及する操作で逆操作を書き漏らすと
// 元に戻せない壊れた状態になる。プロジェクトは数万要素程度でコピーが十分速いため、操作前の全体を保存する
void EditorDocument::commit(const std::string& label, core::Project edited) {
    undo_.push_back(Snapshot{label, std::move(project_), revision_});
    if (undo_.size() > undoLimit_) {
        undo_.pop_front();
    }
    redo_.clear();
    project_ = std::move(edited);
    revision_ = nextRevision_++;
    notify();
}

void EditorDocument::undo() {
    if (undo_.empty()) {
        return;
    }
    Snapshot previous = std::move(undo_.back());
    undo_.pop_back();
    redo_.push_back(Snapshot{previous.label, std::move(project_), revision_});
    project_ = std::move(previous.project);
    revision_ = previous.revision;
    notify();
}

void EditorDocument::redo() {
    if (redo_.empty()) {
        return;
    }
    Snapshot next = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back(Snapshot{next.label, std::move(project_), revision_});
    project_ = std::move(next.project);
    revision_ = next.revision;
    notify();
}

void EditorDocument::replace(core::Project project, std::optional<std::filesystem::path> path) {
    project_ = std::move(project);
    path_ = std::move(path);
    undo_.clear();
    redo_.clear();
    revision_ = nextRevision_++;
    savedRevision_ = revision_;
    notify();
}

void EditorDocument::markSaved(const std::filesystem::path& path) {
    path_ = path;
    savedRevision_ = revision_;
    notify();
}

std::size_t EditorDocument::addListener(Listener listener) {
    const std::size_t id = nextListenerId_++;
    listeners_.emplace_back(id, std::move(listener));
    return id;
}

void EditorDocument::removeListener(std::size_t id) {
    std::erase_if(listeners_, [id](const auto& entry) { return entry.first == id; });
}

void EditorDocument::notify() const {
    for (const auto& [id, listener] : listeners_) {
        listener();
    }
}

}  // namespace ibs::app
