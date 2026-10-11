#include "AppIcon.hpp"

#include <algorithm>

#include <QFile>
#include <QFont>
#include <QPainter>

#include "StudioPalette.hpp"

namespace ibs::gui {

namespace {

// Why not アイコンがないときは空にする: アイコンは後から用意するため、それまでもバージョン情報や
// タスクバーで何のアプリか分かるよう、文字だけの仮のアイコンを描く
QPixmap placeholder(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette::kRuler);
    painter.drawRoundedRect(QRectF(0, 0, size, size), size * 0.18, size * 0.18);
    painter.setBrush(palette::kAutoCut);
    painter.drawRect(QRectF(size * 0.12, size * 0.72, size * 0.76, size * 0.06));
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(std::max(6, size * 32 / 100));
    painter.setFont(font);
    painter.setPen(palette::kText);
    painter.drawText(QRectF(0, 0, size, size * 0.72), Qt::AlignCenter, QStringLiteral("IBS"));
    return pixmap;
}

}  // namespace

bool hasCustomAppIcon() { return QFile::exists(kAppIconResource); }

QPixmap appIconPixmap(int size) {
    if (hasCustomAppIcon()) {
        const QPixmap custom(kAppIconResource);
        if (!custom.isNull()) {
            return custom.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    return placeholder(size);
}

QIcon appIcon() {
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addPixmap(appIconPixmap(size));
    }
    return icon;
}

}  // namespace ibs::gui
