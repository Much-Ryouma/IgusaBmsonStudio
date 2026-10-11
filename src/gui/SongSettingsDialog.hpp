#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <QDialog>

#include "StudioController.hpp"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace ibs::gui {

struct SongSettingsContext {
    std::filesystem::path chartDirectory;
    std::vector<std::string> modeHints;
    std::optional<double> suggestedTotal;
};

class SongSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Purpose { NewProject, EditSong };

    SongSettingsDialog(Purpose purpose, const core::Project& current, SongSettingsContext context = {},
                       QWidget* parent = nullptr);

    [[nodiscard]] NewProjectSettings result() const;
    [[nodiscard]] QLineEdit* imageField(const QString& key) const;
    [[nodiscard]] QLineEdit* titleField() const noexcept { return title_; }
    void acceptImage(const QString& key, const std::filesystem::path& file);

private:
    void browse();
    void validate();
    QWidget* buildImagesTab(const core::SongInfo& info);

    Purpose purpose_;
    core::SongInfo baseInfo_;
    SongSettingsContext context_;
    QLineEdit* path_ = nullptr;
    QLineEdit* title_ = nullptr;
    QLineEdit* subtitle_ = nullptr;
    QLineEdit* artist_ = nullptr;
    QPlainTextEdit* subartists_ = nullptr;
    QLineEdit* genre_ = nullptr;
    QComboBox* modeHint_ = nullptr;
    QLineEdit* chartName_ = nullptr;
    QSpinBox* level_ = nullptr;
    QDoubleSpinBox* judgeRank_ = nullptr;
    QDoubleSpinBox* total_ = nullptr;
    QDoubleSpinBox* bpm_ = nullptr;
    QSpinBox* resolution_ = nullptr;
    QLabel* imageMessage_ = nullptr;
    std::map<QString, QLineEdit*> images_;
};

}  // namespace ibs::gui
