#include "BgaPanel.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "QtText.hpp"
#include "ibs/app/AssetImportService.hpp"
#include "ibs/app/BgaCommands.hpp"

namespace ibs::gui {

namespace {

constexpr int kThumbnailSize = 48;

QIcon thumbnailFor(const std::filesystem::path& file) {
    QImageReader reader(toQString(file));
    reader.setScaledSize(QSize(kThumbnailSize, kThumbnailSize * 3 / 4));
    const QImage image = reader.read();
    if (image.isNull()) {
        // Why not 動画の 1 フレーム目を表示する: Qt Multimedia を依存に加えるほどの価値がなく、BGA の動画は名前で見分けられるため
        QPixmap placeholder(kThumbnailSize, kThumbnailSize * 3 / 4);
        placeholder.fill(QColor(0x50, 0x46, 0x64));
        return QIcon(placeholder);
    }
    return QIcon(QPixmap::fromImage(image));
}

}  // namespace

BgaPanel::BgaPanel(StudioController& controller, QWidget* parent) : QWidget(parent), controller_(controller) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto* caption = new QLabel(QStringLiteral("BGA 画像"), this);
    caption->setToolTip(QStringLiteral("一覧で選んだ画像を、譜面の BGA / LYR / POOR 列でダブルクリックして置きます"));
    layout->addWidget(caption);
    list_ = new QListWidget(this);
    list_->setIconSize(QSize(kThumbnailSize, kThumbnailSize * 3 / 4));
    layout->addWidget(list_, 1);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("追加…"), this);
    remove_ = new QPushButton(QStringLiteral("削除"), this);
    remove_->setToolTip(QStringLiteral("画像と、その画像を使っているイベントを消します"));
    buttons->addWidget(add);
    buttons->addWidget(remove_);
    layout->addLayout(buttons);

    connect(add, &QPushButton::clicked, this, &BgaPanel::browse);
    connect(remove_, &QPushButton::clicked, this, &BgaPanel::removeSelected);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        controller_.selectBgaImage(row >= 0 ? std::optional<std::uint32_t>(list_->item(row)->data(Qt::UserRole).toUInt())
                                            : std::nullopt);
    });
    connect(&controller_, &StudioController::documentChanged, this, &BgaPanel::refresh);
    connect(&controller_, &StudioController::selectionChanged, this,
            [this] { remove_->setEnabled(controller_.selectedBgaImage().has_value()); });
    refresh();
}

void BgaPanel::refresh() {
    const QSignalBlocker blocker(list_);
    const auto selected = controller_.selectedBgaImage();
    list_->clear();
    const auto directory = controller_.document().chartDirectory();
    for (const auto& header : controller_.project().bga.headers) {
        auto* item = new QListWidgetItem(thumbnailFor(directory / toPath(toQString(header.name))),
                                         QStringLiteral("%1: %2").arg(header.id).arg(toQString(header.name)), list_);
        item->setData(Qt::UserRole, header.id);
        if (selected == header.id) {
            list_->setCurrentItem(item);
        }
    }
    remove_->setEnabled(selected.has_value());
}

bool BgaPanel::addImage(const std::filesystem::path& file) {
    const auto name = app::assetNameInside(controller_.document().chartDirectory(), file);
    if (!controller_.document().path() || !name) {
        emit controller_.statusMessage(QStringLiteral("BGA は譜面のフォルダの中にあるファイルを選んでください（相対パスで保存するため）"));
        return false;
    }
    try {
        controller_.selectBgaImage(app::addBgaImage(controller_.document(), *name));
        refresh();
        return true;
    } catch (const std::exception& error) {
        emit controller_.statusMessage(toQString(error.what()));
        return false;
    }
}

void BgaPanel::browse() {
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("BGA 画像・動画"),
                                                            toQString(controller_.document().chartDirectory()),
                                                            QStringLiteral("BGA (*.png *.jpg *.jpeg *.bmp *.webm *.mp4 *.mpg *.wmv)"));
    for (const auto& file : files) {
        addImage(toPath(file));
    }
}

void BgaPanel::removeSelected() {
    if (const auto selected = controller_.selectedBgaImage()) {
        try {
            app::removeBgaImage(controller_.document(), *selected);
        } catch (const std::exception& error) {
            emit controller_.statusMessage(toQString(error.what()));
        }
    }
}

}  // namespace ibs::gui
