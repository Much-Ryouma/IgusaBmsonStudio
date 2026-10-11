#include <QApplication>
#include <QStringList>

#include "MainWindow.hpp"
#include "QtProcessLauncher.hpp"
#include "QtText.hpp"
#include "StudioController.hpp"
#include "ibs/app/AssetImportService.hpp"
#include "ibs/app/ChartStorage.hpp"
#include "ibs/app/SlicingService.hpp"
#include "ibs/infra/audio/MiniaudioOutput.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"
#include "ibs/infra/mode/KeyModeJsonLoader.hpp"

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IgusaBmsonStudio"));

    ibs::app::LocalChartStorage storage;
    ibs::app::LocalAudioSource audioSource;
    ibs::infra::MiniaudioOutput output;
    ibs::app::LocalAssetFileStore assetFiles;
    ibs::core::KeyModeCatalog modes;
    try {
        modes = ibs::infra::loadKeyModeCatalog(ibs::infra::executableDirectory() / "modes");
    } catch (const std::exception&) {
        // Why not 起動を止める: モード定義がなくても音切りと配置はでき、保存時の検証で V 系の警告として分かるため
    }

    ibs::gui::QtProcessLauncher launcher;
    ibs::app::LocalProcessRunner runner;
    ibs::gui::StudioController controller(
        ibs::gui::StudioPorts{storage, audioSource, output, assetFiles, std::move(modes), launcher, runner});
    ibs::gui::MainWindow window(controller);
    window.show();

    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) {
        window.openChart(ibs::gui::toPath(arguments.at(1)));
    }
    return QApplication::exec();
}
