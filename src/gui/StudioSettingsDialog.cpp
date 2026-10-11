#include "StudioSettingsDialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "QtText.hpp"

namespace ibs::gui {

namespace {

const QString kExecutableKey = QStringLiteral("preview/executable");
const QString kTemplateKey = QStringLiteral("preview/commandTemplate");
const QString kFoldKey = QStringLiteral("preview/foldBgmColumns");

}  // namespace

StudioSettingsDialog::StudioSettingsDialog(const app::PlayerSettings& current, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("設定"));
    resize(640, 0);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    auto* row = new QHBoxLayout;
    executable_ = new QLineEdit(toQString(current.executable), this);
    executable_->setPlaceholderText(QStringLiteral("例: C:\\Games\\Pulsus\\Pulsus.exe"));
    auto* browse = new QPushButton(QStringLiteral("参照…"), this);
    row->addWidget(executable_);
    row->addWidget(browse);
    form->addRow(QStringLiteral("プレーヤー"), row);

    auto* templateRow = new QHBoxLayout;
    commandTemplate_ = new QLineEdit(toQString(current.commandTemplate), this);
    auto* reset = new QPushButton(QStringLiteral("既定に戻す"), this);
    templateRow->addWidget(commandTemplate_);
    templateRow->addWidget(reset);
    form->addRow(QStringLiteral("起動コマンド"), templateRow);
    auto* help = new QLabel(QStringLiteral("置換子: {exe} プレーヤー、{file} 譜面、{measure} 小節番号（0〜999）、{pulse}、{seconds}\n"
                                           "Pulsus は \"{exe}\" \"{file}\" -p -m {measure}"),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    foldBgmColumns_ = new QCheckBox(QStringLiteral("BGM 列（x が 9 以上）のノーツを BGM に戻して渡す（Pulsus はこれがあると落ちるため）"), this);
    foldBgmColumns_->setChecked(current.foldBgmColumns);
    layout->addWidget(foldBgmColumns_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("プレーヤー"), executable_->text(),
                                                          QStringLiteral("実行ファイル (*.exe);;すべて (*)"));
        if (!file.isEmpty()) {
            executable_->setText(file);
        }
    });
    connect(reset, &QPushButton::clicked, this, [this] { commandTemplate_->setText(toQString(app::PlayerSettings{}.commandTemplate)); });
}

app::PlayerSettings StudioSettingsDialog::result() const {
    return app::PlayerSettings{toUtf8(executable_->text().trimmed()), toUtf8(commandTemplate_->text().trimmed()),
                               foldBgmColumns_->isChecked()};
}

app::PlayerSettings StudioSettingsDialog::load() {
    const QSettings settings(QStringLiteral("IgusaBmsonStudio"), QStringLiteral("IgusaBmsonStudio"));
    app::PlayerSettings player;
    player.executable = toUtf8(settings.value(kExecutableKey).toString());
    player.commandTemplate = toUtf8(settings.value(kTemplateKey, toQString(player.commandTemplate)).toString());
    player.foldBgmColumns = settings.value(kFoldKey, true).toBool();
    return player;
}

void StudioSettingsDialog::save(const app::PlayerSettings& player) {
    QSettings settings(QStringLiteral("IgusaBmsonStudio"), QStringLiteral("IgusaBmsonStudio"));
    settings.setValue(kExecutableKey, toQString(player.executable));
    settings.setValue(kTemplateKey, toQString(player.commandTemplate));
    settings.setValue(kFoldKey, player.foldBgmColumns);
}

}  // namespace ibs::gui
