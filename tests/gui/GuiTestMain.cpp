#include <QApplication>

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    // Why not 画面のある環境で動かす: CI には表示先がなく、手元でもテストのたびにウィンドウが出ると作業の邪魔になるため
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
