#include "SongSettingsDialog.hpp"

#include <cmath>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "QtText.hpp"
#include "ibs/app/AssetImportService.hpp"

namespace ibs::gui {

namespace {

struct ImageField {
    const char* key;
    const char* label;
    const char* filter;
    std::string core::SongInfo::*member;
};

const ImageField kImageFields[] = {
    {"back_image", "背景（back_image）", "画像 (*.png *.jpg *.jpeg *.bmp)", &core::SongInfo::backImage},
    {"eyecatch_image", "アイキャッチ（eyecatch_image）", "画像 (*.png *.jpg *.jpeg *.bmp)", &core::SongInfo::eyecatchImage},
    {"banner_image", "バナー（banner_image。15:4）", "画像 (*.png *.jpg *.jpeg *.bmp)", &core::SongInfo::bannerImage},
    {"title_image", "タイトル画像（title_image）", "画像 (*.png *.jpg *.jpeg *.bmp)", &core::SongInfo::titleImage},
    {"preview_music", "プレビュー音源（preview_music）", "音声 (*.wav *.ogg)", &core::SongInfo::previewMusic},
};

}  // namespace

SongSettingsDialog::SongSettingsDialog(Purpose purpose, const core::Project& current, SongSettingsContext context,
                                       QWidget* parent)
    : QDialog(parent), purpose_(purpose), baseInfo_(current.info), context_(std::move(context)) {
    setWindowTitle(purpose == Purpose::NewProject ? QStringLiteral("新しい譜面") : QStringLiteral("曲の情報"));
    resize(560, 0);
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    auto* basic = new QWidget(tabs);
    auto* form = new QFormLayout(basic);
    if (purpose == Purpose::NewProject) {
        auto* row = new QHBoxLayout;
        path_ = new QLineEdit(this);
        path_->setPlaceholderText(QStringLiteral("素材の wav と同じフォルダに置く .bmson"));
        auto* browseButton = new QPushButton(QStringLiteral("参照…"), this);
        row->addWidget(path_);
        row->addWidget(browseButton);
        form->addRow(QStringLiteral("保存先"), row);
        connect(browseButton, &QPushButton::clicked, this, &SongSettingsDialog::browse);
        connect(path_, &QLineEdit::textChanged, this, &SongSettingsDialog::validate);
    }

    const auto& info = current.info;
    title_ = new QLineEdit(toQString(info.title), this);
    subtitle_ = new QLineEdit(toQString(info.subtitle), this);
    artist_ = new QLineEdit(toQString(info.artist), this);
    subartists_ = new QPlainTextEdit(this);
    QStringList subartistLines;
    for (const auto& line : info.subartists) {
        subartistLines << toQString(line);
    }
    subartists_->setPlainText(subartistLines.join(QLatin1Char('\n')));
    subartists_->setPlaceholderText(QStringLiteral("1 行に 1 つ。例: movie:作者名、obj:作者名"));
    subartists_->setFixedHeight(64);
    genre_ = new QLineEdit(toQString(info.genre), this);
    modeHint_ = new QComboBox(this);
    modeHint_->setEditable(true);
    for (const auto& hint : context_.modeHints) {
        modeHint_->addItem(toQString(hint));
    }
    modeHint_->setCurrentText(toQString(info.modeHint));
    chartName_ = new QLineEdit(toQString(info.chartName), this);
    level_ = new QSpinBox(this);
    level_->setRange(0, 999);
    level_->setValue(static_cast<int>(info.level));
    judgeRank_ = new QDoubleSpinBox(this);
    judgeRank_->setRange(1.0, 1000.0);
    judgeRank_->setValue(info.judgeRank);
    auto* totalRow = new QHBoxLayout;
    total_ = new QDoubleSpinBox(this);
    total_->setRange(1.0, 100000.0);
    total_->setDecimals(1);
    total_->setValue(info.total);
    totalRow->addWidget(total_);
    if (context_.suggestedTotal) {
        auto* suggest = new QPushButton(QStringLiteral("目安 %1 を入れる").arg(std::lround(*context_.suggestedTotal)), this);
        connect(suggest, &QPushButton::clicked, this, [this] { total_->setValue(std::round(*context_.suggestedTotal)); });
        totalRow->addWidget(suggest);
    }
    bpm_ = new QDoubleSpinBox(this);
    bpm_->setRange(1.0, 999.0);
    bpm_->setDecimals(3);
    bpm_->setValue(current.timing.initialBpm);
    resolution_ = new QSpinBox(this);
    resolution_->setRange(1, 9600);
    resolution_->setValue(current.timing.resolution);
    // Why not resolution を後から変えられるようにする: 変えると既存の切断点とノーツの pulse をすべて換算し直す必要があり、
    // 丸めで位置がずれるため、作成時だけ決める
    resolution_->setEnabled(purpose == Purpose::NewProject);

    form->addRow(QStringLiteral("タイトル"), title_);
    form->addRow(QStringLiteral("サブタイトル"), subtitle_);
    form->addRow(QStringLiteral("アーティスト"), artist_);
    form->addRow(QStringLiteral("サブアーティスト"), subartists_);
    form->addRow(QStringLiteral("ジャンル"), genre_);
    form->addRow(QStringLiteral("モード（mode_hint）"), modeHint_);
    form->addRow(QStringLiteral("譜面名"), chartName_);
    form->addRow(QStringLiteral("レベル"), level_);
    form->addRow(QStringLiteral("判定（judge_rank）"), judgeRank_);
    form->addRow(QStringLiteral("total"), totalRow);
    form->addRow(QStringLiteral("初期 BPM"), bpm_);
    form->addRow(QStringLiteral("resolution（4 分音符あたり）"), resolution_);
    tabs->addTab(basic, QStringLiteral("基本"));
    tabs->addTab(buildImagesTab(info), QStringLiteral("画像・プレビュー音源"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    validate();
}

QWidget* SongSettingsDialog::buildImagesTab(const core::SongInfo& info) {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* form = new QFormLayout;
    layout->addLayout(form);
    for (const auto& field : kImageFields) {
        const QString key = QString::fromLatin1(field.key);
        auto* row = new QHBoxLayout;
        auto* edit = new QLineEdit(toQString(info.*field.member), page);
        auto* browseButton = new QPushButton(QStringLiteral("参照…"), page);
        row->addWidget(edit);
        row->addWidget(browseButton);
        form->addRow(toQString(field.label), row);
        images_[key] = edit;
        const QString filter = toQString(field.filter);
        connect(browseButton, &QPushButton::clicked, this, [this, key, filter] {
            const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("ファイルを選ぶ"),
                                                              toQString(context_.chartDirectory), filter);
            if (!file.isEmpty()) {
                acceptImage(key, toPath(file));
            }
        });
    }
    imageMessage_ = new QLabel(page);
    imageMessage_->setWordWrap(true);
    layout->addWidget(imageMessage_);
    layout->addStretch(1);
    if (purpose_ == Purpose::NewProject) {
        imageMessage_->setText(QStringLiteral("画像は譜面を作った後に「曲の情報」で設定できます（譜面のフォルダが決まってから）"));
        page->setEnabled(false);
    }
    return page;
}

QLineEdit* SongSettingsDialog::imageField(const QString& key) const {
    const auto found = images_.find(key);
    return found == images_.end() ? nullptr : found->second;
}

// Why not 絶対パスのまま保存する: bmson は曲のフォルダごと配布するため、譜面のフォルダからの相対パスにしないと
// 他の人の環境で画像が見つからない（V03）
void SongSettingsDialog::acceptImage(const QString& key, const std::filesystem::path& file) {
    const auto name = app::assetNameInside(context_.chartDirectory, file);
    if (!name) {
        imageMessage_->setText(QStringLiteral("譜面のフォルダの中にあるファイルを選んでください（相対パスで保存するため）"));
        return;
    }
    imageMessage_->clear();
    if (auto* edit = imageField(key)) {
        edit->setText(toQString(*name));
    }
}

void SongSettingsDialog::browse() {
    const QString file = QFileDialog::getSaveFileName(this, QStringLiteral("譜面の保存先"), path_->text(),
                                                      QStringLiteral("bmson (*.bmson)"));
    if (!file.isEmpty()) {
        path_->setText(file.endsWith(QStringLiteral(".bmson"), Qt::CaseInsensitive) ? file : file + QStringLiteral(".bmson"));
    }
}

void SongSettingsDialog::validate() {
    auto* buttons = findChild<QDialogButtonBox*>();
    if (buttons != nullptr && path_ != nullptr) {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!path_->text().trimmed().isEmpty());
    }
}

NewProjectSettings SongSettingsDialog::result() const {
    NewProjectSettings settings;
    if (path_ != nullptr) {
        settings.chartPath = toPath(path_->text().trimmed());
    }
    settings.info = baseInfo_;
    settings.info.title = toUtf8(title_->text());
    settings.info.subtitle = toUtf8(subtitle_->text());
    settings.info.artist = toUtf8(artist_->text());
    settings.info.subartists.clear();
    for (const auto& line : subartists_->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (!line.trimmed().isEmpty()) {
            settings.info.subartists.push_back(toUtf8(line.trimmed()));
        }
    }
    settings.info.genre = toUtf8(genre_->text());
    settings.info.modeHint = toUtf8(modeHint_->currentText().trimmed());
    settings.info.chartName = toUtf8(chartName_->text());
    settings.info.level = static_cast<std::uint32_t>(level_->value());
    settings.info.judgeRank = judgeRank_->value();
    settings.info.total = total_->value();
    for (const auto& field : kImageFields) {
        settings.info.*field.member = toUtf8(images_.at(QString::fromLatin1(field.key))->text().trimmed());
    }
    settings.initialBpm = bpm_->value();
    settings.resolution = resolution_->value();
    return settings;
}

}  // namespace ibs::gui
