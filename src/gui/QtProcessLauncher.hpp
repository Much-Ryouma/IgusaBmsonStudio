#pragma once

#include <QProcess>
#include <QStringList>

#include "QtText.hpp"
#include "ibs/app/PreviewService.hpp"

namespace ibs::gui {

class QtProcessLauncher final : public app::ProcessLauncher {
public:
    bool launch(const app::LaunchRequest& request) override {
        QStringList arguments;
        for (const auto& argument : request.arguments) {
            arguments << toQString(argument);
        }
        // Why not QProcess を待つ: プレーヤーは独立したウィンドウとして遊ぶものなので、終了を待たずに切り離して起動する
        return QProcess::startDetached(toQString(request.program), arguments, toQString(request.workingDirectory));
    }
};

}  // namespace ibs::gui
