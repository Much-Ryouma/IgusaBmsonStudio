#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include "ibs/app/AssetImportService.hpp"
#include "ibs/app/AudioLibrary.hpp"
#include "ibs/app/ChartCommands.hpp"
#include "ibs/app/ChartStatisticsService.hpp"
#include "ibs/app/ChartService.hpp"
#include "ibs/app/EditorDocument.hpp"
#include "ibs/app/NullTestService.hpp"
#include "ibs/app/PlaybackService.hpp"
#include "ibs/app/PreviewService.hpp"
#include "ibs/core/model/BgaEditing.hpp"
#include "ibs/app/SlicingSession.hpp"
#include "ibs/app/TimelineViewport.hpp"

namespace ibs::gui {

struct StudioPorts {
    app::ChartStorage& storage;
    app::AudioSource& audioSource;
    core::AudioOutputPort& output;
    app::AssetFileStore& assetFiles;
    core::KeyModeCatalog modes;
    app::ProcessLauncher& launcher;
    app::ProcessRunner& runner;
};

enum class NullTestSource { InternalPlayer, ExternalPlayer };

struct BgaEventRef {
    core::BgaTrack track = core::BgaTrack::Bga;
    core::Pulse y = 0;

    friend bool operator==(const BgaEventRef&, const BgaEventRef&) = default;
};

struct NewProjectSettings {
    std::filesystem::path chartPath;
    core::SongInfo info;
    double initialBpm = 120.0;
    int resolution = core::kDefaultResolution;
};

class StudioController final : public QObject {
    Q_OBJECT

public:
    explicit StudioController(StudioPorts ports, QObject* parent = nullptr);
    ~StudioController() override;

    [[nodiscard]] app::EditorDocument& document() noexcept { return document_; }
    [[nodiscard]] const core::Project& project() const noexcept { return document_.project(); }
    [[nodiscard]] app::AudioLibrary& library() noexcept { return library_; }
    [[nodiscard]] app::SlicingSession& slicing() noexcept { return slicing_; }
    [[nodiscard]] app::PlaybackService& playback() noexcept { return playback_; }
    [[nodiscard]] const app::MixSettings& mix() const noexcept { return mix_; }
    [[nodiscard]] const core::KeyMode* keyMode() const;
    [[nodiscard]] std::vector<std::string> modeHints() const;

    void newProject(const NewProjectSettings& settings);
    bool open(const std::filesystem::path& path, QString* error);
    app::SaveOutcome save(const std::filesystem::path& path);
    app::ImportOutcome importAudio(const std::filesystem::path& file, app::OutsideFilePolicy policy);
    app::BgaImportOutcome importBga(const std::filesystem::path& file, app::OutsideFilePolicy policy);
    [[nodiscard]] std::vector<std::string> unreachableBgaFiles() const;
    void updateSongInfo(const core::SongInfo& info, double initialBpm);

    [[nodiscard]] std::optional<core::PlacementId> selectedPlacement() const noexcept { return selectedPlacement_; }
    void selectPlacement(std::optional<core::PlacementId> placement);
    [[nodiscard]] const std::vector<core::CutPointId>& selectedCuts() const noexcept { return selectedCuts_; }
    void setSelectedCuts(std::vector<core::CutPointId> cuts);
    void focusCuts(std::vector<core::CutPointId> cuts);
    [[nodiscard]] bool followsChartSelection() const noexcept { return followChartSelection_; }
    void setFollowsChartSelection(bool enabled) { followChartSelection_ = enabled; }
    [[nodiscard]] bool followsPlayback() const noexcept { return followPlayback_; }
    void setFollowsPlayback(bool enabled) { followPlayback_ = enabled; }
    [[nodiscard]] const std::vector<core::NoteId>& selectedNotes() const noexcept { return selectedNotes_; }
    void setSelectedNotes(std::vector<core::NoteId> notes);

    void copySelectedNotes();
    core::PasteResult pasteAtCursor();
    [[nodiscard]] bool hasCopiedPattern() const noexcept { return !clipboard_.entries.empty(); }
    app::TimingEditOutcome editTiming(const QString& label, const std::function<void(core::TimingData&)>& edit);

    [[nodiscard]] const app::PlayerSettings& playerSettings() const noexcept { return playerSettings_; }
    void setPlayerSettings(app::PlayerSettings settings) { playerSettings_ = std::move(settings); }
    app::PreviewOutcome previewInPlayer();
    void requestStatistics();
    void requestNullTest(NullTestSource source);

    [[nodiscard]] std::optional<std::uint32_t> selectedBgaImage() const noexcept { return selectedBgaImage_; }
    void selectBgaImage(std::optional<std::uint32_t> id);
    [[nodiscard]] std::optional<BgaEventRef> selectedBgaEvent() const noexcept { return selectedBgaEvent_; }
    void selectBgaEvent(std::optional<BgaEventRef> event);

    [[nodiscard]] int snapDivisions() const noexcept { return snapDivisions_; }
    void setSnapDivisions(int divisions);
    [[nodiscard]] core::Pulse snap(double pulse, app::SnapMode mode) const;
    [[nodiscard]] static app::SnapMode snapModeFor(Qt::KeyboardModifiers modifiers);

    [[nodiscard]] core::Pulse cursorPulse() const noexcept { return cursorPulse_; }
    void setCursorPulse(core::Pulse pulse);

    void toggleMute(core::AssetId asset);
    void toggleSolo(core::AssetId asset);

    [[nodiscard]] app::SliceSettings sliceSettings() const;
    void setSliceSettings(const app::SliceSettings& settings);
    [[nodiscard]] bool autoPreview() const noexcept { return autoPreview_; }
    void setAutoPreview(bool enabled);
    void requestPreview();
    void clearPreview();
    [[nodiscard]] const std::optional<app::SlicePreview>& preview() const noexcept { return preview_; }
    app::SliceOutcome applyPreview();
    [[nodiscard]] bool isAnalyzing() const noexcept { return analyzing_; }

    void playSlice(core::CutPointId cut);
    void playPlacement(core::PlacementId placement);
    void playSongFromCursor();
    void stopPlayback();
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] std::optional<double> playheadPulse() const;
    [[nodiscard]] std::optional<double> songPlayheadPulse() const;

    void waitForBackgroundWork();

signals:
    void documentChanged();
    void selectionChanged();
    void previewChanged();
    void audioLoaded(const QString& assetName);
    void playheadChanged();
    void cursorChanged();
    void mixChanged();
    void analysisStateChanged(bool analyzing);
    void statusMessage(const QString& message);
    void tempoDesynced(const app::TimingEditOutcome& outcome);
    void cutFocusRequested(core::CutPointId cut);
    void statisticsReady(const app::ChartStatisticsReport& report);
    void statisticsFailed(const QString& reason);
    void nullTestReady(const app::NullTestReport& report);
    void nullTestFailed(const QString& reason);

private:
    void onDocumentChanged();
    void resetForNewDocument();
    void preloadAudio();
    void refreshPreviewFromCache();
    void reportPlayback(const app::PlaybackOutcome& outcome, bool wholeSong);

    app::ChartStorage& storage_;
    app::AudioSource& audioSource_;
    app::ProcessRunner& runner_;
    core::AudioOutputPort& output_;
    app::ChartService charts_;
    app::EditorDocument document_;
    app::AudioLibrary library_;
    app::SlicingSession slicing_;
    app::PlaybackService playback_;
    app::AssetImportService importer_;
    app::PreviewService previewer_;
    app::PlayerSettings playerSettings_;
    std::optional<std::uint32_t> selectedBgaImage_;
    std::optional<BgaEventRef> selectedBgaEvent_;
    app::MixSettings mix_;
    std::optional<core::PlacementId> selectedPlacement_;
    std::vector<core::CutPointId> selectedCuts_;
    std::vector<core::NoteId> selectedNotes_;
    core::NotePattern clipboard_;
    int snapDivisions_ = 16;
    core::Pulse cursorPulse_ = 0;
    bool autoPreview_ = true;
    bool followChartSelection_ = true;
    bool followPlayback_ = true;
    bool playingWholeSong_ = false;
    bool analyzing_ = false;
    std::uint64_t analysisGeneration_ = 0;
    std::uint64_t documentGeneration_ = 0;
    std::optional<app::SlicePreview> preview_;
    QTimer playheadTimer_;
    QThreadPool pool_;
};

}  // namespace ibs::gui
