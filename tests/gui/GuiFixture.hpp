#pragma once

#include <QApplication>
#include <QMouseEvent>
#include <QWidget>

#include "../unit/support/Beat7k.hpp"
#include "../unit/support/FakeAppPorts.hpp"
#include "../unit/support/SyntheticAudio.hpp"
#include "ArrangerWidget.hpp"
#include "MainWindow.hpp"
#include "StudioController.hpp"
#include "WaveformSlicerWidget.hpp"
#include "ibs/app/ArrangementCommands.hpp"

namespace ibs::gui::testing {

inline const std::filesystem::path kChartDirectory = "chart";

inline core::AudioBuffer drumHits() {
    using ibs::testing::samplesAt;
    std::vector<float> signal(samplesAt(2.0), 0.0f);
    std::uint32_t seed = 1;
    for (const double at : {0.5, 1.0, 1.5}) {
        ibs::testing::addNoiseBurst(signal, samplesAt(at), 0.5, 40.0, seed++);
    }
    return core::AudioBuffer{ibs::testing::kTestSampleRate, 1, std::move(signal)};
}

inline core::KeyModeCatalog beat7kCatalog() {
    core::KeyModeCatalog catalog;
    catalog.add(ibs::testing::beat7k());
    return catalog;
}

class GuiFixture {
public:
    GuiFixture()
        : controller_(StudioPorts{storage_, source_, output_, files_, beat7kCatalog(), launcher_, runner_}), window_(controller_) {
        source_.buffers[kChartDirectory / "drums.wav"] = drumHits();
        NewProjectSettings settings;
        settings.chartPath = kChartDirectory / "song.bmson";
        settings.initialBpm = 120.0;
        settings.resolution = 240;
        controller_.newProject(settings);
        asset = controller_.document().apply("素材", [](core::Project& project) { return project.addAsset("drums.wav"); });
        placement = app::placeAsset(controller_.document(), asset, 960);
        window_.resize(1200, 900);
        window_.show();
        settle();
    }

    void settle() {
        for (int round = 0; round < 50; ++round) {
            controller_.waitForBackgroundWork();
            QApplication::processEvents();
            if (!controller_.isAnalyzing()) {
                break;
            }
        }
        QApplication::processEvents();
    }

    static void send(QWidget* widget, QEvent::Type type, QPointF position, Qt::MouseButton button,
                     Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent event(type, position, widget->mapToGlobal(position), button, buttons, modifiers);
        QApplication::sendEvent(widget, &event);
    }

    static void drag(QWidget* widget, QPointF from, QPointF to, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        send(widget, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton, modifiers);
        send(widget, QEvent::MouseMove, (from + to) / 2.0, Qt::NoButton, Qt::LeftButton, modifiers);
        send(widget, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton, modifiers);
        send(widget, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton, modifiers);
    }

    static void doubleClick(QWidget* widget, QPointF at, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        send(widget, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton, modifiers);
        send(widget, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton, modifiers);
        send(widget, QEvent::MouseButtonDblClick, at, Qt::LeftButton, Qt::LeftButton, modifiers);
        send(widget, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton, modifiers);
    }

    [[nodiscard]] QPointF slicerPoint(double songPulse) const {
        return QPointF(window_.slicer()->xOfSongPulse(songPulse), window_.slicer()->height() / 2.0);
    }

    StudioController& controller() { return controller_; }
    MainWindow& window() { return window_; }
    ibs::testing::FakeAudioOutput& output() { return output_; }
    ibs::testing::RecordingLauncher& launcher() { return launcher_; }
    ibs::testing::RecordingRunner& runner() { return runner_; }
    const core::Project& project() const { return controller_.project(); }

    core::AssetId asset;
    core::PlacementId placement;

private:
    ibs::testing::FilesOnlyStorage storage_{{"drums.wav"}};
    ibs::testing::InMemoryAudioSource source_;
    ibs::testing::FakeAudioOutput output_{1000};
    ibs::testing::FakeAssetFileStore files_;
    ibs::testing::RecordingLauncher launcher_;
    ibs::testing::RecordingRunner runner_;
    StudioController controller_;
    MainWindow window_;
};

}  // namespace ibs::gui::testing
