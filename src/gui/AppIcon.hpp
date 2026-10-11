#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace ibs::gui {

inline const QString kAppIconResource = QStringLiteral(":/resources/icons/app.png");

[[nodiscard]] bool hasCustomAppIcon();
[[nodiscard]] QPixmap appIconPixmap(int size);
[[nodiscard]] QIcon appIcon();

}  // namespace ibs::gui
