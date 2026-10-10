#include "StudioController.hpp"

#include <algorithm>
#include <exception>

#include <QMetaObject>
#include <QPointer>
#include <QRunnable>

#include "QtText.hpp"

namespace ibs::gui {

namespace {

constexpr int kPlayheadIntervalMs = 30;

template <typename Work>
void runInPool(QThreadPool& pool, Work work) {
    pool.start(QRunnable::create(std::move(work)));
}

bool containsCut(const core::Project& project, core::CutPointId id) {
    const auto& cuts = project.cutPoints();
    return std::any_of(cuts.begin(), cuts.end(), [id](const core::CutPoint& cut) { return cut.id == id; });
}

bool containsPlacement(const core::Project& project, core::PlacementId id) {
    const auto& placements = project.placements();
    return std::any_of(placements.begin(), placements.end(), [id](const core::Placement& placement) { return placement.id == id; });
}

}  // namespace

StudioController::StudioController(StudioPorts ports, QObject* parent)
    : QObject(parent),
      storage_(ports.storage),
      output_(ports.output),
      charts_(ports.storage, std::move(ports.modes)),
      library_(ports.storage, ports.audioSource),
      slicing_(document_, library_),
      playback_(library_, ports.output),
      importer_(ports.assetFiles) {
    // Why not スレッド数を CPU に任せる: 数分のステムを同時にいくつもデコードするとメモリを大きく使い、
    // 画面の描画まで遅くなるため、同時に処理する素材を 2 つまでにする
    pool_.setMaxThreadCount(2);
    document_.addListener([this] { onDocumentChanged(); });
    playheadTimer_.setInterval(kPlayheadIntervalMs);
    connect(&playheadTimer_, &QTimer::timeout, this, [this] {
        if (!isPlaying()) {
            playheadTimer_.stop();
        }
        emit playheadChanged();
    });
}

StudioController::~StudioController() {
    output_.stop();
    pool_.waitForDone();
}

void StudioController::waitForBackgroundWork() { pool_.waitForDone(); }

void StudioController::resetForNewDocument() {
    stopPlayback();
    selectedPlacement_.reset();
    selectedCuts_.clear();
    preview_.reset();
    mix_ = {};
    cursorPulse_ = 0;
    ++analysisGeneration_;
    ++documentGeneration_;
    analyzing_ = false;
    library_.setChartDirectory(document_.chartDirectory());
    library_.clear();
    emit selectionChanged();
    emit previewChanged();
    emit mixChanged();
    emit cursorChanged();
    preloadAudio();
}

void StudioController::newProject(const NewProjectSettings& settings) {
    core::Project project;
    project.info = settings.info;
    project.timing.initialBpm = settings.initialBpm;
    project.timing.resolution = settings.resolution;
    document_.replace(std::move(project), settings.chartPath);
    resetForNewDocument();
}

bool StudioController::open(const std::filesystem::path& path, QString* error) {
    auto outcome = charts_.open(path);
    if (!outcome.chart) {
        if (error != nullptr) {
            *error = toQString(outcome.error);
        }
        return false;
    }
    document_.replace(std::move(outcome.chart->project), path);
    resetForNewDocument();
    if (!outcome.chart->notices.empty()) {
        emit statusMessage(QStringLiteral("読み込み時の通知が %1 件あります").arg(outcome.chart->notices.size()));
    }
    return true;
}

app::SaveOutcome StudioController::save(const std::filesystem::path& path) {
    auto outcome = charts_.save(document_.project(), path);
    if (outcome.saved) {
        const bool moved = document_.chartDirectory() != path.parent_path();
        document_.markSaved(path);
        if (moved) {
            library_.setChartDirectory(document_.chartDirectory());
            preloadAudio();
        }
    }
    return outcome;
}

app::ImportOutcome StudioController::importAudio(const std::filesystem::path& file, app::OutsideFilePolicy policy) {
    auto outcome = importer_.import(document_, file, policy);
    if (outcome.assetId) {
        library_.invalidate(outcome.assetName);
        preloadAudio();
    }
    return outcome;
}

void StudioController::updateSongInfo(const core::SongInfo& info, double initialBpm) {
    if (info == document_.project().info && initialBpm == document_.project().timing.initialBpm) {
        return;
    }
    document_.apply("曲の情報", [&](core::Project& project) {
        project.info = info;
        project.timing.initialBpm = initialBpm;
    });
}

void StudioController::preloadAudio() {
    const auto generation = documentGeneration_;
    for (const auto& asset : document_.project().assets()) {
        if (library_.cached(asset.name)) {
            continue;
        }
        const std::string name = asset.name;
        QPointer<StudioController> self(this);
        runInPool(pool_, [this, self, name, generation] {
            static_cast<void>(library_.get(name));
            QMetaObject::invokeMethod(this, [self, name, generation] {
                if (self && generation == self->documentGeneration_) {
                    emit self->audioLoaded(toQString(name));
                    emit self->documentChanged();
                }
            }, Qt::QueuedConnection);
        });
    }
}

void StudioController::onDocumentChanged() {
    const auto& project = document_.project();
    if (selectedPlacement_ && !containsPlacement(project, *selectedPlacement_)) {
        selectedPlacement_.reset();
        preview_.reset();
        emit previewChanged();
    }
    std::erase_if(selectedCuts_, [&project](core::CutPointId id) { return !containsCut(project, id); });
    emit documentChanged();
    emit selectionChanged();
    if (preview_) {
        refreshPreviewFromCache();
    }
}

void StudioController::selectPlacement(std::optional<core::PlacementId> placement) {
    if (placement == selectedPlacement_) {
        return;
    }
    selectedPlacement_ = placement;
    selectedCuts_.clear();
    preview_.reset();
    emit selectionChanged();
    emit previewChanged();
    if (placement && autoPreview_) {
        requestPreview();
    }
}

void StudioController::setSelectedCuts(std::vector<core::CutPointId> cuts) {
    selectedCuts_ = std::move(cuts);
    emit selectionChanged();
}

void StudioController::setSnapDivisions(int divisions) { snapDivisions_ = std::max(0, divisions); }

core::Pulse StudioController::snap(double pulse, app::SnapMode mode) const {
    return app::snapPulse(document_.project().timing, pulse, snapDivisions_, mode);
}

app::SnapMode StudioController::snapModeFor(Qt::KeyboardModifiers modifiers) {
    if (modifiers.testFlag(Qt::AltModifier)) {
        return app::SnapMode::Off;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        return app::SnapMode::Fine;
    }
    return app::SnapMode::Grid;
}

void StudioController::setCursorPulse(core::Pulse pulse) {
    cursorPulse_ = std::max<core::Pulse>(0, pulse);
    emit cursorChanged();
}

void StudioController::toggleMute(core::AssetId asset) {
    if (!mix_.muted.erase(asset)) {
        mix_.muted.insert(asset);
    }
    emit mixChanged();
}

void StudioController::toggleSolo(core::AssetId asset) {
    if (!mix_.soloed.erase(asset)) {
        mix_.soloed.insert(asset);
    }
    emit mixChanged();
}

app::SliceSettings StudioController::sliceSettings() const {
    return selectedPlacement_ ? slicing_.settingsFor(*selectedPlacement_) : app::SliceSettings{};
}

void StudioController::setSliceSettings(const app::SliceSettings& settings) {
    if (!selectedPlacement_) {
        return;
    }
    slicing_.setSettings(*selectedPlacement_, settings);
    if (autoPreview_) {
        requestPreview();
    }
}

void StudioController::setAutoPreview(bool enabled) {
    autoPreview_ = enabled;
    if (enabled && selectedPlacement_) {
        requestPreview();
    }
}

void StudioController::refreshPreviewFromCache() {
    if (!selectedPlacement_) {
        return;
    }
    const auto& project = document_.project();
    const auto settings = slicing_.settingsFor(*selectedPlacement_);
    const auto& name = project.asset(project.placement(*selectedPlacement_).assetId).name;
    const auto analysis = library_.cachedAnalysis(name, settings.onset);
    if (!analysis) {
        return;
    }
    try {
        preview_ = app::SlicingSession::previewWith(project, *selectedPlacement_, *analysis, settings);
    } catch (const std::exception& error) {
        preview_.reset();
        emit statusMessage(toQString(error.what()));
    }
    emit previewChanged();
}

// Why not GUI スレッドで解析する: 数分のステムでは解析に 1 秒前後かかり、その間スライダーも描画も止まるため、
// 解析だけを裏で行い、閾値の調整（拾い直し）は解析結果を使い回して即座に反映する
void StudioController::requestPreview() {
    if (!selectedPlacement_) {
        return;
    }
    const auto& project = document_.project();
    const auto settings = slicing_.settingsFor(*selectedPlacement_);
    const std::string name = project.asset(project.placement(*selectedPlacement_).assetId).name;
    if (library_.cachedAnalysis(name, settings.onset)) {
        if (analyzing_) {
            ++analysisGeneration_;
            analyzing_ = false;
            emit analysisStateChanged(false);
        }
        refreshPreviewFromCache();
        return;
    }
    const auto generation = ++analysisGeneration_;
    analyzing_ = true;
    emit analysisStateChanged(true);
    QPointer<StudioController> self(this);
    runInPool(pool_, [this, self, name, settings, generation] {
        std::string error;
        try {
            if (!library_.analysis(name, settings.onset)) {
                error = library_.get(name).error;
            }
        } catch (const std::exception& exception) {
            error = exception.what();
        }
        QMetaObject::invokeMethod(this, [self, generation, error] {
            if (!self || generation != self->analysisGeneration_) {
                return;
            }
            self->analyzing_ = false;
            emit self->analysisStateChanged(false);
            if (!error.empty()) {
                emit self->statusMessage(toQString(error));
                return;
            }
            self->refreshPreviewFromCache();
        }, Qt::QueuedConnection);
    });
}

void StudioController::clearPreview() {
    preview_.reset();
    emit previewChanged();
}

app::SliceOutcome StudioController::applyPreview() {
    if (!preview_) {
        app::SliceOutcome outcome;
        outcome.error = "適用する検出結果がありません";
        return outcome;
    }
    const auto preview = *preview_;
    auto outcome = slicing_.apply(preview);
    if (outcome.succeeded) {
        refreshPreviewFromCache();
    }
    return outcome;
}

void StudioController::reportPlayback(const app::PlaybackOutcome& outcome) {
    if (outcome.started) {
        playheadTimer_.start();
    } else if (!outcome.error.empty()) {
        emit statusMessage(toQString(outcome.error));
    }
    emit playheadChanged();
}

void StudioController::playSlice(core::CutPointId cut) { reportPlayback(playback_.playSlice(document_.project(), cut)); }

void StudioController::playPlacement(core::PlacementId placement) {
    reportPlayback(playback_.playPlacement(document_.project(), placement));
}

void StudioController::playSongFromCursor() {
    reportPlayback(playback_.playSong(document_.project(), cursorPulse_, mix_));
}

void StudioController::stopPlayback() {
    playback_.stop();
    playheadTimer_.stop();
    emit playheadChanged();
}

bool StudioController::isPlaying() const { return playback_.isPlaying(); }

std::optional<double> StudioController::playheadPulse() const {
    const auto seconds = playback_.playheadSongSeconds();
    if (!seconds) {
        return std::nullopt;
    }
    return document_.project().timing.toTimingMap().pulseAt(*seconds);
}

}  // namespace ibs::gui
