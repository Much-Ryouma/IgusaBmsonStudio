#include "SongSettingsDialog.hpp"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "QtText.hpp"

namespace ibs::gui {

SongSettingsDialog::SongSettingsDialog(Purpose purpose, const core::Project& current, QWidget* parent)
    : QDialog(parent), purpose_(purpose), baseInfo_(current.info) {
    setWindowTitle(purpose == Purpose::NewProject ? QStringLiteral("新しい譜面") : QStringLiteral("曲の情報"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    if (purpose == Purpose::NewProject) {
        auto* row = new QHBoxLayout;
        path_ = new QLineEdit(this);
        path_->setPlaceholderText(QStringLiteral("素材の wav と同じフォルダに置く .bmson"));
        auto* browse = new QPushButton(QStringLiteral("参照…"), this);
        row->addWidget(path_);
        row->addWidget(browse);
        form->addRow(QStringLiteral("保存先"), row);
        connect(browse, &QPushButton::clicked, this, &SongSettingsDialog::browse);
        connect(path_, &QLineEdit::textChanged, this, &SongSettingsDialog::validate);
    }

    title_ = new QLineEdit(toQString(current.info.title), this);
    subtitle_ = new QLineEdit(toQString(current.info.subtitle), this);
    artist_ = new QLineEdit(toQString(current.info.artist), this);
    genre_ = new QLineEdit(toQString(current.info.genre), this);
    chartName_ = new QLineEdit(toQString(current.info.chartName), this);
    level_ = new QSpinBox(this);
    level_->setRange(0, 99);
    level_->setValue(static_cast<int>(current.info.level));
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
    form->addRow(QStringLiteral("ジャンル"), genre_);
    form->addRow(QStringLiteral("譜面名"), chartName_);
    form->addRow(QStringLiteral("レベル"), level_);
    form->addRow(QStringLiteral("初期 BPM"), bpm_);
    form->addRow(QStringLiteral("resolution（4 分音符あたり）"), resolution_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    validate();
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
    settings.info.genre = toUtf8(genre_->text());
    settings.info.chartName = toUtf8(chartName_->text());
    settings.info.level = static_cast<std::uint32_t>(level_->value());
    settings.initialBpm = bpm_->value();
    settings.resolution = resolution_->value();
    return settings;
}

}  // namespace ibs::gui
