#pragma once

#include <memory>

#include <QSettings>

namespace ibs::gui {

// Why not QSettings(組織名, アプリ名) を直接作る: その形は既定の保存形式を無視するため、
// テストで設定の保存先を一時フォルダの ini に向けられず、手元の設定を書き換えてしまう
[[nodiscard]] inline std::unique_ptr<QSettings> openStudioSettings() {
    return std::make_unique<QSettings>(QSettings::defaultFormat(), QSettings::UserScope, QStringLiteral("IgusaBmsonStudio"),
                                       QStringLiteral("IgusaBmsonStudio"));
}

}  // namespace ibs::gui
