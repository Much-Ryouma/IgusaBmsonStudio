#include "StudioSettings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace ibs::gui {

namespace {

QString& overriddenFile() {
    static QString file;
    return file;
}

}  // namespace

// Why not OS のユーザー設定（レジストリ）に保存する: zip を展開して使うだけのアプリなので、フォルダを消せば
// 何も残らないようにし、困ったときは利用者がテキストエディタで直せるよう exe の横の ini に置く
QString studioSettingsFile() {
    if (!overriddenFile().isEmpty()) {
        return overriddenFile();
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(kSettingsFileName);
}

void setStudioSettingsFile(const QString& file) { overriddenFile() = file; }

std::unique_ptr<QSettings> openStudioSettings() {
    return std::make_unique<QSettings>(studioSettingsFile(), QSettings::IniFormat);
}

bool studioSettingsWritable() {
    const QFileInfo file(studioSettingsFile());
    return file.exists() ? file.isWritable() : QFileInfo(file.absolutePath()).isWritable();
}

}  // namespace ibs::gui
