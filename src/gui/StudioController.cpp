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
      audioSource_(ports.audioSource),
      runner_(ports.runner),
      output_(ports.output),
      charts_(ports.storage, std::move(ports.modes)),
      library_(ports.storage, ports.audioSource),
      slicing_(document_, library_),
      playback_(library_, ports.output),
      importer_(ports.assetFiles),
      previewer_(charts_, ports.launcher) {
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
    selectedNotes_.clear();
    preview_.reset();
    selectedBgaImage_.reset();
    selectedBgaEvent_.reset();
    slicing_.replaceAllSettings({});
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
    slicing_.replaceAllSettings(std::move(outcome.chart->sliceSettings));
    if (!outcome.chart->notices.empty()) {
        QString first = toQString(outcome.chart->notices.front().message);
        emit statusMessage(QStringLiteral("読み込み時の通知が %1 件あります（%2 …）").arg(outcome.chart->notices.size()).arg(first));
    }
    return true;
}

app::SaveOutcome StudioController::save(const std::filesystem::path& path) {
    auto outcome = charts_.save(document_.project(), path, slicing_.allSettings());
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
    std::erase_if(selectedNotes_, [&project](core::NoteId id) { return !project.containsNote(id); });
    if (selectedBgaImage_ && std::none_of(project.bga.headers.begin(), project.bga.headers.end(),
                                          [this](const core::BgaHeader& header) { return header.id == *selectedBgaImage_; })) {
        selectedBgaImage_.reset();
    }
    if (selectedBgaEvent_) {
        const auto& events = core::eventsOf(project.bga, selectedBgaEvent_->track);
        if (std::none_of(events.begin(), events.end(), [this](const core::BgaEvent& event) { return event.y == selectedBgaEvent_->y; })) {
            selectedBgaEvent_.reset();
        }
    }
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

// Why not 譜面で選んだ切断点の配置を必ず波形に出す: 譜面を打つ作業中に下の波形が毎回切り替わるのを嫌う人もいるため、
// 「クリックしたスライスを波形に表示」を切れるようにし、切ったときは選択だけを変える
void StudioController::focusCuts(std::vector<core::CutPointId> cuts) {
    if (!cuts.empty() && followChartSelection_) {
        const auto placement = document_.project().cutPoint(cuts.front()).placementId;
        if (placement != selectedPlacement_) {
            selectedPlacement_ = placement;
            preview_.reset();
            emit previewChanged();
            if (autoPreview_) {
                requestPreview();
            }
        }
    }
    selectedCuts_ = std::move(cuts);
    emit selectionChanged();
    if (!selectedCuts_.empty() && followChartSelection_) {
        emit cutFocusRequested(selectedCuts_.front());
    }
}

void StudioController::setSelectedNotes(std::vector<core::NoteId> notes) {
    selectedNotes_ = std::move(notes);
    emit selectionChanged();
}

const core::KeyMode* StudioController::keyMode() const { return charts_.keyModeFor(document_.project()); }

std::vector<std::string> StudioController::modeHints() const {
    std::vector<std::string> hints;
    for (const auto& mode : charts_.modes().modes()) {
        hints.push_back(mode.modeHint());
    }
    return hints;
}

void StudioController::copySelectedNotes() {
    clipboard_ = core::copyPattern(document_.project(), selectedNotes_);
    emit statusMessage(QStringLiteral("%1 個のノーツの割当をコピーしました").arg(clipboard_.entries.size()));
}

core::PasteResult StudioController::pasteAtCursor() {
    const auto result = app::pastePattern(document_, clipboard_, cursorPulse_);
    setSelectedNotes(result.addedNotes);
    QString message = QStringLiteral("貼り付け: 追加 %1").arg(result.added);
    if (result.missingCutPoints > 0) {
        message += QStringLiteral(" / 同じ位置に切断点がない %1").arg(result.missingCutPoints);
    }
    if (result.alreadyPresent > 0) {
        message += QStringLiteral(" / すでにある %1").arg(result.alreadyPresent);
    }
    emit statusMessage(message);
    return result;
}

app::TimingEditOutcome StudioController::editTiming(const QString& label,
                                                    const std::function<void(core::TimingData&)>& edit) {
    auto outcome = app::editTiming(document_, toUtf8(label), edit);
    if (!outcome.desynced.empty()) {
        emit tempoDesynced(outcome);
    }
    return outcome;
}

app::PreviewOutcome StudioController::previewInPlayer() {
    auto outcome = previewer_.preview(document_, cursorPulse_, playerSettings_);
    if (!outcome.launched) {
        emit statusMessage(toQString(outcome.error));
    } else {
        QString message = QStringLiteral("プレーヤーを起動しました: 小節 %1 から（%2）")
                              .arg(app::previewPositionAt(document_.project(), cursorPulse_).measure)
                              .arg(toQString(outcome.file.filename()));
        if (outcome.foldedBgmNotes > 0) {
            message += QStringLiteral("。BGM 列のノーツ %1 個は BGM に戻して渡しました").arg(outcome.foldedBgmNotes);
        }
        emit statusMessage(message);
    }
    return outcome;
}

// Why not GUI スレッドで計算する: 素材をすべて鳴らし直して音量を測るため、数分の曲では数秒かかり、その間画面が止まる
void StudioController::requestStatistics() {
    const auto* mode = keyMode();
    if (mode == nullptr) {
        emit statisticsFailed(QStringLiteral("mode_hint のモード定義がないため、統計を出せません"));
        return;
    }
    QPointer<StudioController> self(this);
    runInPool(pool_, [this, self, project = document_.project(), mode = *mode] {
        std::map<std::string, std::shared_ptr<const app::LoadedAudio>> keep;
        std::map<std::string, const core::AudioBuffer*> audio;
        for (const auto& asset : project.assets()) {
            if (auto loaded = library_.get(asset.name).audio) {
                audio.emplace(asset.name, &loaded->audio);
                keep.emplace(asset.name, std::move(loaded));
            }
        }
        try {
            auto report = app::computeChartStatistics(project, mode, audio);
            QMetaObject::invokeMethod(this, [self, report = std::move(report)] {
                if (self) {
                    emit self->statisticsReady(report);
                }
            }, Qt::QueuedConnection);
        } catch (const std::exception& error) {
            const QString reason = toQString(error.what());
            QMetaObject::invokeMethod(this, [self, reason] {
                if (self) {
                    emit self->statisticsFailed(reason);
                }
            }, Qt::QueuedConnection);
        }
    });
}

// Why not 読み込み済みの素材を使う: ヌルテストは保存した譜面がどう鳴るかを確かめるもので、
// 編集中にまだ保存していない素材の差し替えを混ぜないよう、譜面の隣にあるファイルを読み直す
void StudioController::requestNullTest(NullTestSource source) {
    if (!document_.path()) {
        emit nullTestFailed(QStringLiteral("ヌルテストの前に譜面を保存してください"));
        return;
    }
    if (source == NullTestSource::ExternalPlayer && playerSettings_.executable.empty()) {
        emit nullTestFailed(QStringLiteral("設定でプレーヤーの実行ファイルを指定してください"));
        return;
    }
    QPointer<StudioController> self(this);
    runInPool(pool_, [this, self, source, project = document_.project(), path = *document_.path(), player = playerSettings_] {
        const app::NullTestService service(charts_, storage_, audioSource_);
        auto report = source == NullTestSource::InternalPlayer
                          ? service.compareWithInternalPlayer(project, path.parent_path())
                          : service.compareWithPlayer(project, path, player, player.renderTemplate, runner_);
        QMetaObject::invokeMethod(this, [self, report = std::move(report)] {
            if (!self) {
                return;
            }
            if (report.completed) {
                emit self->nullTestReady(report);
            } else {
                emit self->nullTestFailed(toQString(report.error));
            }
        }, Qt::QueuedConnection);
    });
}

void StudioController::selectBgaImage(std::optional<std::uint32_t> id) {
    selectedBgaImage_ = id;
    emit selectionChanged();
}

void StudioController::selectBgaEvent(std::optional<BgaEventRef> event) {
    selectedBgaEvent_ = event;
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

void StudioController::reportPlayback(const app::PlaybackOutcome& outcome, bool wholeSong) {
    if (outcome.started) {
        playingWholeSong_ = wholeSong;
        playheadTimer_.start();
    } else if (!outcome.error.empty()) {
        emit statusMessage(toQString(outcome.error));
    }
    emit playheadChanged();
}

void StudioController::playSlice(core::CutPointId cut) { reportPlayback(playback_.playSlice(document_.project(), cut), false); }

void StudioController::playPlacement(core::PlacementId placement) {
    reportPlayback(playback_.playPlacement(document_.project(), placement), false);
}

void StudioController::playSongFromCursor() {
    reportPlayback(playback_.playSong(document_.project(), cursorPulse_, mix_), true);
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

// Why not 試聴でもアレンジャーと譜面に再生位置を出す: スライスの試聴は波形を見ながら行うもので、
// そのたびに上の画面の再生位置やスクロールまで動くと、編集中の場所を見失うため
std::optional<double> StudioController::songPlayheadPulse() const {
    return playingWholeSong_ ? playheadPulse() : std::nullopt;
}

}  // namespace ibs::gui
