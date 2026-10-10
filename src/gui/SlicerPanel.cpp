#include "SlicerPanel.hpp"

#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "QtText.hpp"

namespace ibs::gui {

namespace {

constexpr int kSliderSteps = 100;
constexpr int kGridChoices[] = {4, 8, 12, 16, 24, 32, 48, 64};

QDoubleSpinBox* makeSpin(double minimum, double maximum, double step, const QString& suffix, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setDecimals(1);
    spin->setSuffix(suffix);
    return spin;
}

}  // namespace

SlicerPanel::SlicerPanel(StudioController& controller, QWidget* parent) : QWidget(parent), controller_(controller) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    function_ = new QComboBox(this);
    function_->addItem(QStringLiteral("スペクトルフラックス"), static_cast<int>(core::OnsetFunction::SpectralFlux));
    function_->addItem(QStringLiteral("エネルギー（打楽器向け）"), static_cast<int>(core::OnsetFunction::Energy));
    function_->addItem(QStringLiteral("HFC（高域）"), static_cast<int>(core::OnsetFunction::HighFrequencyContent));
    form->addRow(QStringLiteral("検出方法"), function_);

    auto* sensitivityRow = new QHBoxLayout;
    sensitivity_ = new QSlider(Qt::Horizontal, this);
    sensitivity_->setRange(0, kSliderSteps);
    sensitivityValue_ = new QLabel(this);
    sensitivityValue_->setMinimumWidth(32);
    sensitivityRow->addWidget(sensitivity_);
    sensitivityRow->addWidget(sensitivityValue_);
    form->addRow(QStringLiteral("感度"), sensitivityRow);

    minimumInterval_ = makeSpin(0.0, 1000.0, 5.0, QStringLiteral(" ms"), this);
    form->addRow(QStringLiteral("最小間隔"), minimumInterval_);
    silenceGate_ = makeSpin(-120.0, 0.0, 1.0, QStringLiteral(" dB"), this);
    form->addRow(QStringLiteral("無音ゲート"), silenceGate_);
    preroll_ = makeSpin(0.0, 100.0, 0.5, QStringLiteral(" ms"), this);
    form->addRow(QStringLiteral("プリロール"), preroll_);

    grid_ = new QComboBox(this);
    grid_->addItem(QStringLiteral("なし"), 0);
    for (const int divisions : kGridChoices) {
        grid_->addItem(QStringLiteral("%1 分").arg(divisions), divisions);
    }
    form->addRow(QStringLiteral("クオンタイズ"), grid_);
    strength_ = new QSlider(Qt::Horizontal, this);
    strength_->setRange(0, kSliderSteps);
    form->addRow(QStringLiteral("強さ"), strength_);
    window_ = makeSpin(0.0, 500.0, 1.0, QStringLiteral(" ms"), this);
    window_->setSpecialValueText(QStringLiteral("制限なし"));
    form->addRow(QStringLiteral("寄せる範囲"), window_);

    autoPreview_ = new QCheckBox(QStringLiteral("設定を変えたらすぐ検出する"), this);
    autoPreview_->setChecked(controller_.autoPreview());
    layout->addWidget(autoPreview_);

    auto* buttons = new QHBoxLayout;
    previewButton_ = new QPushButton(QStringLiteral("検出"), this);
    applyButton_ = new QPushButton(QStringLiteral("適用"), this);
    applyButton_->setToolTip(QStringLiteral("自動検出の切断点（レーン未割当）を、表示中の検出結果で置き換えます"));
    buttons->addWidget(previewButton_);
    buttons->addWidget(applyButton_);
    layout->addLayout(buttons);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    layout->addStretch(1);

    setSettings(controller_.sliceSettings());

    connect(function_, &QComboBox::currentIndexChanged, this, &SlicerPanel::onEdited);
    connect(sensitivity_, &QSlider::valueChanged, this, &SlicerPanel::onEdited);
    connect(minimumInterval_, &QDoubleSpinBox::valueChanged, this, &SlicerPanel::onEdited);
    connect(silenceGate_, &QDoubleSpinBox::valueChanged, this, &SlicerPanel::onEdited);
    connect(preroll_, &QDoubleSpinBox::valueChanged, this, &SlicerPanel::onEdited);
    connect(grid_, &QComboBox::currentIndexChanged, this, &SlicerPanel::onEdited);
    connect(strength_, &QSlider::valueChanged, this, &SlicerPanel::onEdited);
    connect(window_, &QDoubleSpinBox::valueChanged, this, &SlicerPanel::onEdited);
    connect(autoPreview_, &QCheckBox::toggled, this, [this](bool enabled) { controller_.setAutoPreview(enabled); });
    connect(previewButton_, &QPushButton::clicked, this, [this] { controller_.requestPreview(); });
    connect(applyButton_, &QPushButton::clicked, this, &SlicerPanel::onApply);

    connect(&controller_, &StudioController::selectionChanged, this, [this] {
        setSettings(controller_.sliceSettings());
        refreshEnabled();
    });
    connect(&controller_, &StudioController::previewChanged, this, &SlicerPanel::refreshStatus);
    connect(&controller_, &StudioController::analysisStateChanged, this, &SlicerPanel::refreshStatus);
    refreshEnabled();
    refreshStatus();
}

app::SliceSettings SlicerPanel::settings() const {
    app::SliceSettings settings;
    settings.onset.function = static_cast<core::OnsetFunction>(function_->currentData().toInt());
    settings.onset.sensitivity = sensitivity_->value() / static_cast<double>(kSliderSteps);
    settings.onset.minimumIntervalMs = minimumInterval_->value();
    settings.onset.silenceGateDb = silenceGate_->value();
    settings.onset.prerollMs = preroll_->value();
    settings.quantize.divisionsPerWholeNote = grid_->currentData().toInt();
    settings.quantize.strength = strength_->value() / static_cast<double>(kSliderSteps);
    if (window_->value() > 0.0) {
        settings.quantize.windowMs = window_->value();
    }
    return settings;
}

void SlicerPanel::setSettings(const app::SliceSettings& settings) {
    updating_ = true;
    function_->setCurrentIndex(function_->findData(static_cast<int>(settings.onset.function)));
    sensitivity_->setValue(static_cast<int>(std::lround(settings.onset.sensitivity * kSliderSteps)));
    sensitivityValue_->setText(QString::number(settings.onset.sensitivity, 'f', 2));
    minimumInterval_->setValue(settings.onset.minimumIntervalMs);
    silenceGate_->setValue(settings.onset.silenceGateDb);
    preroll_->setValue(settings.onset.prerollMs);
    const int gridIndex = grid_->findData(settings.quantize.divisionsPerWholeNote);
    grid_->setCurrentIndex(gridIndex >= 0 ? gridIndex : 0);
    strength_->setValue(static_cast<int>(std::lround(settings.quantize.strength * kSliderSteps)));
    window_->setValue(settings.quantize.windowMs.value_or(0.0));
    updating_ = false;
}

void SlicerPanel::onEdited() {
    sensitivityValue_->setText(QString::number(sensitivity_->value() / static_cast<double>(kSliderSteps), 'f', 2));
    if (!updating_) {
        controller_.setSliceSettings(settings());
    }
}

void SlicerPanel::onApply() {
    const auto outcome = controller_.applyPreview();
    if (!outcome.succeeded) {
        status_->setText(toQString(outcome.error));
        return;
    }
    QString message = QStringLiteral("適用しました: 追加 %1 / 削除 %2").arg(outcome.applied.added).arg(outcome.applied.removed);
    if (!outcome.issues.empty()) {
        message += QStringLiteral("\nアタック欠けの警告 (V09) が %1 件あります（赤い線）").arg(outcome.issues.size());
    }
    emit controller_.statusMessage(message);
}

void SlicerPanel::refreshEnabled() {
    const bool hasPlacement = controller_.selectedPlacement().has_value();
    for (QWidget* widget : std::initializer_list<QWidget*>{function_, sensitivity_, minimumInterval_, silenceGate_, preroll_,
                                                           grid_, strength_, window_, previewButton_}) {
        widget->setEnabled(hasPlacement);
    }
    applyButton_->setEnabled(hasPlacement && controller_.preview().has_value());
}

void SlicerPanel::refreshStatus() {
    refreshEnabled();
    if (controller_.isAnalyzing()) {
        status_->setText(QStringLiteral("解析中…"));
        return;
    }
    const auto& preview = controller_.preview();
    if (!preview) {
        status_->setText(controller_.selectedPlacement() ? QStringLiteral("「検出」で切断点の候補を表示します")
                                                         : QStringLiteral("配置を選んでください"));
        return;
    }
    std::size_t clipped = 0;
    for (const auto& cut : preview->plan.cuts) {
        clipped += cut.attackClipped ? 1u : 0u;
    }
    status_->setText(QStringLiteral("立ち上がり %1 / 切断点の候補 %2 / 置き換わる自動の切断点 %3 / アタック欠け %4")
                         .arg(preview->detectedOnsets)
                         .arg(preview->plan.cuts.size())
                         .arg(preview->replaced.size())
                         .arg(clipped));
}

}  // namespace ibs::gui
