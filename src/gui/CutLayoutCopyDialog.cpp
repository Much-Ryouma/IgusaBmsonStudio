#include "CutLayoutCopyDialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtText.hpp"
#include "ibs/core/chart/CutLayout.hpp"
#include "ibs/core/timing/TimingEdits.hpp"

namespace ibs::gui {

namespace {

constexpr int kAssetRole = Qt::UserRole + 1;
constexpr int kListedStarts = 6;

QString startsText(const core::Project& project, core::AssetId asset) {
    QStringList bars;
    const auto starts = core::placementStartsOf(project, asset);
    for (std::size_t index = 0; index < starts.size() && index < kListedStarts; ++index) {
        bars << QStringLiteral("小節 %1（y=%2）").arg(core::barIndexAt(project.timing, starts[index])).arg(starts[index]);
    }
    if (starts.size() > kListedStarts) {
        bars << QStringLiteral("ほか %1 か所").arg(starts.size() - kListedStarts);
    }
    return bars.join(QStringLiteral("、"));
}

}  // namespace

CutLayoutCopyDialog::CutLayoutCopyDialog(const core::Project& project, core::AssetId source, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("音源配置をコピー"));
    resize(520, 420);
    auto* layout = new QVBoxLayout(this);
    const auto& name = project.asset(source).name;
    auto* intro = new QLabel(QStringLiteral("「%1」の切断点の位置を、下で選んだ素材にコピーします。\n"
                                            "配置の位置: %2\n"
                                            "同じ位置にすべての配置がある素材だけが一覧に出ます。")
                                 .arg(toQString(name), startsText(project, source)),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    list_ = new QListWidget(this);
    for (const auto target : core::assetsWithSamePlacements(project, source)) {
        std::size_t cuts = 0;
        std::size_t notes = 0;
        for (const auto& placement : project.placements()) {
            if (placement.assetId != target) {
                continue;
            }
            for (const auto cut : project.cutPointsOf(placement.id)) {
                cuts += project.cutPoint(cut).offsetPulses > 0 ? 1 : 0;
                notes += project.notesOf(cut).size();
            }
        }
        auto* item = new QListWidgetItem(QStringLiteral("%1（今の切断点 %2・ノーツ %3）")
                                             .arg(toQString(project.asset(target).name))
                                             .arg(cuts)
                                             .arg(notes),
                                         list_);
        item->setData(kAssetRole, static_cast<qulonglong>(target.value));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    layout->addWidget(list_, 1);
    if (list_->count() == 0) {
        layout->addWidget(new QLabel(QStringLiteral("同じ位置に配置された素材がありません。"), this));
    }
    auto* note = new QLabel(QStringLiteral("コピー先の切断点とノーツの割り当ては置き換わります（割り当ては写しません。同じ音が重なって鳴るのを防ぐため）。"), this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_ = buttons->button(QDialogButtonBox::Ok);
    ok_->setText(QStringLiteral("コピー"));
    ok_->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(list_, &QListWidget::itemChanged, this, [this] { ok_->setEnabled(!chosenTargets().empty()); });
}

std::vector<core::AssetId> CutLayoutCopyDialog::chosenTargets() const {
    std::vector<core::AssetId> targets;
    for (int row = 0; row < list_->count(); ++row) {
        const auto* item = list_->item(row);
        if (item->checkState() == Qt::Checked) {
            targets.push_back(core::AssetId{item->data(kAssetRole).toULongLong()});
        }
    }
    return targets;
}

}  // namespace ibs::gui
