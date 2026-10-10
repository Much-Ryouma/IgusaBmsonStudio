#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include "ibs/app/AssetImportService.hpp"
#include "ibs/app/AudioLibrary.hpp"
#include "ibs/app/ChartService.hpp"
#include "ibs/app/EditorDocument.hpp"
#include "ibs/app/PlaybackService.hpp"
#include "ibs/app/SlicingSession.hpp"
#include "ibs/app/TimelineViewport.hpp"

namespace ibs::gui {

struct StudioPorts {
    app::ChartStorage& storage;
    app::AudioSource& audioSource;
    core::AudioOutputPort& output;
    app::AssetFileStore& assetFiles;
    core::KeyModeCatalog modes;
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

    void newProject(const NewProjectSettings& settings);
    bool open(const std::filesystem::path& path, QString* error);
    app::SaveOutcome save(const std::filesystem::path& path);
    app::ImportOutcome importAudio(const std::filesystem::path& file, app::OutsideFilePolicy policy);
    void updateSongInfo(const core::SongInfo& info, double initialBpm);

    [[nodiscard]] std::optional<core::PlacementId> selectedPlacement() const noexcept { return selectedPlacement_; }
    void selectPlacement(std::optional<core::PlacementId> placement);
    [[nodiscard]] const std::vector<core::CutPointId>& selectedCuts() const noexcept { return selectedCuts_; }
    void setSelectedCuts(std::vector<core::CutPointId> cuts);

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

private:
    void onDocumentChanged();
    void resetForNewDocument();
    void preloadAudio();
    void refreshPreviewFromCache();
    void reportPlayback(const app::PlaybackOutcome& outcome);

    app::ChartStorage& storage_;
    core::AudioOutputPort& output_;
    app::ChartService charts_;
    app::EditorDocument document_;
    app::AudioLibrary library_;
    app::SlicingSession slicing_;
    app::PlaybackService playback_;
    app::AssetImportService importer_;
    app::MixSettings mix_;
    std::optional<core::PlacementId> selectedPlacement_;
    std::vector<core::CutPointId> selectedCuts_;
    int snapDivisions_ = 16;
    core::Pulse cursorPulse_ = 0;
    bool autoPreview_ = true;
    bool analyzing_ = false;
    std::uint64_t analysisGeneration_ = 0;
    std::uint64_t documentGeneration_ = 0;
    std::optional<app::SlicePreview> preview_;
    QTimer playheadTimer_;
    QThreadPool pool_;
};

}  // namespace ibs::gui
