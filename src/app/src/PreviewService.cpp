#include "ibs/app/PreviewService.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>

#include "ibs/core/chart/ChartEditing.hpp"
#include "ibs/core/timing/TimingEdits.hpp"
#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::app {

namespace {

constexpr int kMaximumMeasure = 999;

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
}

std::string formatSeconds(double seconds) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", seconds);
    return buffer;
}

}  // namespace

std::vector<std::string> splitCommandLine(const std::string& commandLine) {
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    bool hasToken = false;
    for (const char character : commandLine) {
        if (character == '"') {
            quoted = !quoted;
            hasToken = true;
        } else if ((character == ' ' || character == '\t') && !quoted) {
            if (hasToken) {
                tokens.push_back(current);
                current.clear();
                hasToken = false;
            }
        } else {
            current.push_back(character);
            hasToken = true;
        }
    }
    if (hasToken) {
        tokens.push_back(current);
    }
    return tokens;
}

// Why not 置換してから空白で区切る: 曲のフォルダ名に空白が入っていると、置換後のパスが途中で切れて
// 別の引数になってしまうため、テンプレートを先に区切ってから各引数の中だけを置換する
LaunchRequest buildLaunchRequest(const PlayerSettings& settings, const std::filesystem::path& file,
                                 const PreviewPosition& position) {
    const std::string fileText = infra::pathToUtf8(file);
    LaunchRequest request;
    request.workingDirectory = file.parent_path();
    auto tokens = splitCommandLine(settings.commandTemplate);
    for (auto& token : tokens) {
        replaceAll(token, "{exe}", settings.executable);
        replaceAll(token, "{file}", fileText);
        replaceAll(token, "{measure}", std::to_string(position.measure));
        replaceAll(token, "{pulse}", std::to_string(position.pulse));
        replaceAll(token, "{seconds}", formatSeconds(position.seconds));
    }
    if (!tokens.empty()) {
        request.program = tokens.front();
        request.arguments.assign(tokens.begin() + 1, tokens.end());
    }
    return request;
}

PreviewPosition previewPositionAt(const core::Project& project, core::Pulse cursorPulse) {
    PreviewPosition position;
    position.pulse = std::max<core::Pulse>(0, cursorPulse);
    position.measure = std::clamp(core::barIndexAt(project.timing, position.pulse), 0, kMaximumMeasure);
    position.seconds = project.timing.toTimingMap().secondsAt(position.pulse);
    return position;
}

// Why not OS の一時フォルダに書き出す: bmson の素材は譜面からの相対パスで書かれているため、
// 別のフォルダに置くとプレーヤーが音を見つけられない
std::filesystem::path previewFileFor(const std::filesystem::path& chartPath) {
    return chartPath.parent_path() / infra::pathFromUtf8(".~preview_" + infra::pathToUtf8(chartPath.filename()));
}

PreviewService::PreviewService(const ChartService& charts, ProcessLauncher& launcher)
    : charts_(charts), launcher_(launcher) {}

PreviewOutcome PreviewService::preview(const EditorDocument& document, core::Pulse cursorPulse,
                                       const PlayerSettings& settings) {
    PreviewOutcome outcome;
    try {
        if (settings.executable.empty()) {
            outcome.error = "プレーヤーの実行ファイルが設定されていません（設定 → プレビュー）";
            return outcome;
        }
        if (!document.path()) {
            outcome.error = "譜面の保存先が決まっていません";
            return outcome;
        }
        outcome.file = *document.path();
        core::Project project = document.project();
        // Why not BGM 列（x が 9 以上）をそのまま渡す: Pulsus は x が 0 以外のノーツをすべて鍵盤として扱い、
        // レーンの配列の外を読んで落ちる（Pulsus 0.5.3、Skin.cs の laneWidths は 9 要素）ため、プレビュー用の一時ファイルでだけ BGM に戻す
        if (const auto* mode = charts_.keyModeFor(project); mode != nullptr && settings.foldBgmColumns) {
            outcome.foldedBgmNotes = core::foldBgmColumns(project, *mode);
        }
        if (document.isDirty() || outcome.foldedBgmNotes > 0) {
            outcome.file = previewFileFor(*document.path());
            const auto exported = charts_.exportCopy(project, outcome.file);
            outcome.issues = exported.issues;
            if (!exported.saved) {
                outcome.error = exported.error;
                return outcome;
            }
        }
        outcome.request = buildLaunchRequest(settings, outcome.file, previewPositionAt(document.project(), cursorPulse));
        outcome.launched = launcher_.launch(outcome.request);
        if (!outcome.launched) {
            outcome.error = "プレーヤーを起動できません: " + outcome.request.program;
        }
    } catch (const std::exception& error) {
        outcome.error = error.what();
    }
    return outcome;
}

}  // namespace ibs::app
