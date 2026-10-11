#include "AboutDialog.hpp"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "AppIcon.hpp"
#include "QtText.hpp"
#include "ibs/core/BuildInfo.hpp"

namespace ibs::gui {

namespace {

constexpr int kIconSize = 128;

QString readResource(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

}  // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("バージョン情報"));
    resize(620, 520);
    auto* layout = new QVBoxLayout(this);

    auto* header = new QHBoxLayout;
    icon_ = new QLabel(this);
    icon_->setPixmap(appIconPixmap(kIconSize));
    icon_->setFixedSize(kIconSize, kIconSize);
    header->addWidget(icon_, 0, Qt::AlignTop);

    const QString homepage = toQString(core::kHomepageUrl);
    auto* text = new QVBoxLayout;
    auto* name = new QLabel(QStringLiteral("<span style='font-size:20pt; font-weight:bold'>%1</span>")
                                .arg(toQString(core::kApplicationName)),
                            this);
    version_ = new QLabel(QStringLiteral("バージョン %1（Qt %2）").arg(toQString(core::kVersion), QString::fromLatin1(qVersion())), this);
    version_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* description = new QLabel(QStringLiteral("bmson の音切り・譜面編集・プレビューのためのツール"), this);
    auto* links = new QLabel(QStringLiteral("<a href='%1'>GitHub</a>　<a href='%1/wiki'>使い方（Wiki）</a>　"
                                            "<a href='%1/releases'>最新版</a>")
                                 .arg(homepage),
                             this);
    links->setOpenExternalLinks(true);
    auto* copyright = new QLabel(QStringLiteral("Copyright (c) 2026 IgusaBmsonStudio contributors. MIT License."), this);
    text->addWidget(name);
    text->addWidget(version_);
    text->addWidget(description);
    text->addWidget(links);
    text->addWidget(copyright);
    text->addStretch(1);
    header->addLayout(text, 1);
    layout->addLayout(header);

    layout->addWidget(new QLabel(QStringLiteral("使っているソフトウェアとライセンス"), this));
    notices_ = new QTextBrowser(this);
    notices_->setPlainText(readResource(QStringLiteral(":/packaging/THIRD_PARTY_NOTICES.txt")));
    notices_->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(notices_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

}  // namespace ibs::gui
