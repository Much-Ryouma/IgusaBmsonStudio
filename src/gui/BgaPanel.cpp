#include "BgaPanel.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
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

std::vector<std::filesystem::path> droppedBgaFiles(const QMimeData* data) {
    std::vector<std::filesystem::path> files;
    if (data == nullptr) {
        return files;
    }
    for (const auto& url : data->urls()) {
        if (url.isLocalFile() && app::isBgaFile(toPath(url.toLocalFile()))) {
            files.push_back(toPath(url.toLocalFile()));
        }
    }
    return files;
}

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
    setAcceptDrops(true);
    auto* caption = new QLabel(QStringLiteral("BGA 画像"), this);
    caption->setToolTip(QStringLiteral("一覧で選んだ画像を、譜面の BGA / LYR / POOR 列でダブルクリックして置きます。"
                                       "画像・動画のファイルを一覧へドラッグ＆ドロップしても追加できます"));
    layout->addWidget(caption);
    list_ = new QListWidget(this);
    list_->setIconSize(QSize(kThumbnailSize, kThumbnailSize * 3 / 4));
    list_->setToolTip(QStringLiteral("画像・動画のファイルをここへドラッグ＆ドロップすると追加します"));
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
                                                            QStringLiteral("BGA (*.png *.jpg *.jpeg *.bmp *.webm *.mp4 *.mpg *.mpeg *.wmv *.avi)"));
    std::vector<std::filesystem::path> paths;
    for (const auto& file : files) {
        paths.push_back(toPath(file));
    }
    addFiles(paths);
}

// Why not 保存先が決まるまで受け付けない: BGA は譜面からの相対パスで保存するため、譜面の場所が決まらないと
// どのフォルダに置けばよいかが分からない。落とした流れのまま保存先を決めてもらい、続けて追加する
std::size_t BgaPanel::addFiles(const std::vector<std::filesystem::path>& files) {
    if (files.empty()) {
        return 0;
    }
    if (!controller_.document().path()) {
        QMessageBox::warning(this, QStringLiteral("BGA の追加"),
                             QStringLiteral("BGA は譜面（bmson）からの相対パスで保存するため、先に譜面のファイル名と保存先を決めてください。"));
        if (!saveChartAs_ || !saveChartAs_() || !controller_.document().path()) {
            return 0;
        }
    }
    const auto directory = controller_.document().chartDirectory();
    QStringList outside;
    for (const auto& file : files) {
        if (!app::assetNameInside(directory, file)) {
            outside << toQString(file.filename());
        }
    }
    auto policy = app::OutsideFilePolicy::Reject;
    if (!outside.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("譜面のフォルダの外のファイル"),
            QStringLiteral("次のファイルは譜面（bmson）と同じフォルダにありません。譜面のフォルダにコピーして使いますか？\n\n%1")
                .arg(outside.join(QLatin1Char('\n'))));
        policy = answer == QMessageBox::Yes ? app::OutsideFilePolicy::Copy : app::OutsideFilePolicy::Reject;
    }
    std::size_t added = 0;
    QStringList errors;
    for (const auto& file : files) {
        const auto outcome = controller_.importBga(file, policy);
        if (outcome.imageId) {
            added += outcome.alreadyImported ? 0 : 1;
        } else {
            errors << toQString(outcome.error);
        }
    }
    refresh();
    emit controller_.statusMessage(errors.isEmpty() ? QStringLiteral("BGA を %1 個追加しました").arg(added)
                                                    : errors.join(QStringLiteral(" / ")));
    return added;
}

void BgaPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (!droppedBgaFiles(event->mimeData()).empty()) {
        event->acceptProposedAction();
    }
}

void BgaPanel::dragMoveEvent(QDragMoveEvent* event) {
    if (!droppedBgaFiles(event->mimeData()).empty()) {
        event->acceptProposedAction();
    }
}

void BgaPanel::dropEvent(QDropEvent* event) {
    const auto files = droppedBgaFiles(event->mimeData());
    if (files.empty()) {
        return;
    }
    event->acceptProposedAction();
    addFiles(files);
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
