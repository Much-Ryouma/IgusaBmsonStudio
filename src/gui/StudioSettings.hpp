#pragma once

#include <memory>

#include <QSettings>
#include <QString>

namespace ibs::gui {

inline const QString kSettingsFileName = QStringLiteral("IgusaBmsonStudio.ini");

[[nodiscard]] QString studioSettingsFile();
void setStudioSettingsFile(const QString& file);
[[nodiscard]] std::unique_ptr<QSettings> openStudioSettings();
[[nodiscard]] bool studioSettingsWritable();

}  // namespace ibs::gui
