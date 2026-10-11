#include <QApplication>
#include <QDir>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "StudioSettings.hpp"

int main(int argc, char** argv) {
    // Why not 画面のある環境で動かす: CI には表示先がなく、手元でもテストのたびにウィンドウが出ると作業の邪魔になるため
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication application(argc, argv);
    // Why not 本来の設定を使う: テストでの切り替えや最近使った譜面が、手元のアプリの設定に残ってしまうため
    const QTemporaryDir settingsDirectory;
    ibs::gui::setStudioSettingsFile(QDir(settingsDirectory.path()).filePath(QStringLiteral("test.ini")));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
