// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimeStretchDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>

#include <cmath>

namespace dawcast::widgets {

namespace {
// Slider ranges — sliders use integer units
constexpr int kStretchSliderMin =  25;   // 25% = 0.25 ratio
constexpr int kStretchSliderMax = 400;   // 400% = 4.0 ratio
constexpr int kStretchSliderDef = 100;   // 100% = 1.0 ratio

// Pitch slider uses cents for fine control (-1200 to +1200)
constexpr int kPitchSliderMin = -1200;
constexpr int kPitchSliderMax =  1200;
constexpr int kPitchSliderDef =     0;

const QString kDialogStyle = QStringLiteral(
    "QDialog { background: #2a2a30; }"
    "QGroupBox { color: #ccc; border: 1px solid #444; border-radius: 4px; "
    "  margin-top: 12px; padding-top: 16px; font-weight: bold; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
    "QLabel { color: #bbb; font-size: 11px; }"
    "QSlider::groove:horizontal { background: #444; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #8888cc; width: 14px; margin: -5px 0; "
    "  border-radius: 7px; }"
    "QDoubleSpinBox { background: #333; color: #ddd; border: 1px solid #555; "
    "  border-radius: 3px; padding: 2px 4px; }"
    "QCheckBox { color: #bbb; }"
    "QPushButton { background: #3a3a45; color: #ccc; border: 1px solid #555; "
    "  border-radius: 3px; padding: 6px 16px; font-size: 11px; }"
    "QPushButton:hover { background: #4a4a55; }"
    "QPushButton:default { background: #4455aa; color: #fff; border-color: #5566bb; }");
}

// ── Construction ───────────────────────────────────────────────────────────────

TimeStretchDialog::TimeStretchDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Time Stretch / Pitch Shift"));
    setMinimumSize(315, 285);
    setStyleSheet(kDialogStyle);
    buildUI();
}

TimeStretchDialog::~TimeStretchDialog() = default;

// ── Public getters ─────────────────────────────────────────────────────────────

float TimeStretchDialog::stretchRatio() const
{
    return static_cast<float>(m_stretchSpin->value() / 100.0);
}

float TimeStretchDialog::pitchSemitones() const
{
    return static_cast<float>(m_pitchSpin->value());
}

bool TimeStretchDialog::preservePitch() const
{
    return m_preservePitch->isChecked();
}

bool TimeStretchDialog::applyToSelection() const
{
    return m_applySelection->isChecked();
}

// ── UI construction ────────────────────────────────────────────────────────────

void TimeStretchDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // ── Time Stretch group ──
    auto* stretchGroup = new QGroupBox(tr("Time Stretch"), this);
    auto* stretchGrid = new QGridLayout(stretchGroup);
    stretchGrid->setSpacing(8);

    stretchGrid->addWidget(new QLabel(tr("Ratio:"), stretchGroup), 0, 0);

    m_stretchSlider = new QSlider(Qt::Horizontal, stretchGroup);
    m_stretchSlider->setRange(kStretchSliderMin, kStretchSliderMax);
    m_stretchSlider->setValue(kStretchSliderDef);
    stretchGrid->addWidget(m_stretchSlider, 0, 1);

    m_stretchSpin = new QDoubleSpinBox(stretchGroup);
    m_stretchSpin->setRange(25.0, 400.0);
    m_stretchSpin->setValue(100.0);
    m_stretchSpin->setSuffix(QStringLiteral("%"));
    m_stretchSpin->setDecimals(1);
    m_stretchSpin->setSingleStep(1.0);
    m_stretchSpin->setFixedWidth(68);
    stretchGrid->addWidget(m_stretchSpin, 0, 2);

    m_durationLabel = new QLabel(tr("Duration: 1.00x"), stretchGroup);
    m_durationLabel->setStyleSheet(QStringLiteral("color: #999; font-style: italic;"));
    stretchGrid->addWidget(m_durationLabel, 1, 1, 1, 2);

    mainLayout->addWidget(stretchGroup);

    // ── Pitch Shift group ──
    auto* pitchGroup = new QGroupBox(tr("Pitch Shift"), this);
    auto* pitchGrid = new QGridLayout(pitchGroup);
    pitchGrid->setSpacing(8);

    pitchGrid->addWidget(new QLabel(tr("Semitones:"), pitchGroup), 0, 0);

    m_pitchSlider = new QSlider(Qt::Horizontal, pitchGroup);
    m_pitchSlider->setRange(kPitchSliderMin, kPitchSliderMax);
    m_pitchSlider->setValue(kPitchSliderDef);
    pitchGrid->addWidget(m_pitchSlider, 0, 1);

    m_pitchSpin = new QDoubleSpinBox(pitchGroup);
    m_pitchSpin->setRange(-12.0, 12.0);
    m_pitchSpin->setValue(0.0);
    m_pitchSpin->setSuffix(QStringLiteral(" st"));
    m_pitchSpin->setDecimals(2);
    m_pitchSpin->setSingleStep(0.01);
    m_pitchSpin->setFixedWidth(68);
    pitchGrid->addWidget(m_pitchSpin, 0, 2);

    m_pitchLabel = new QLabel(tr("0 cents"), pitchGroup);
    m_pitchLabel->setStyleSheet(QStringLiteral("color: #999; font-style: italic;"));
    pitchGrid->addWidget(m_pitchLabel, 1, 1, 1, 2);

    mainLayout->addWidget(pitchGroup);

    // ── Options group ──
    auto* optionsGroup = new QGroupBox(tr("Options"), this);
    auto* optionsLayout = new QVBoxLayout(optionsGroup);

    m_preservePitch = new QCheckBox(tr("Preserve pitch (uncheck for tape-speed behavior)"),
                                     optionsGroup);
    m_preservePitch->setChecked(true);
    optionsLayout->addWidget(m_preservePitch);

    m_applySelection = new QCheckBox(tr("Apply to selection only"), optionsGroup);
    m_applySelection->setChecked(true);
    optionsLayout->addWidget(m_applySelection);

    mainLayout->addWidget(optionsGroup);

    // ── Buttons ──
    mainLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_previewBtn = new QPushButton(tr("Preview"), this);
    buttonLayout->addWidget(m_previewBtn);

    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    buttonLayout->addWidget(m_cancelBtn);

    m_applyBtn = new QPushButton(tr("Apply"), this);
    m_applyBtn->setDefault(true);
    buttonLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(buttonLayout);

    // ── Connections ──
    connect(m_stretchSlider, &QSlider::valueChanged,
            this, &TimeStretchDialog::onStretchSliderChanged);
    connect(m_stretchSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TimeStretchDialog::onStretchSpinChanged);
    connect(m_pitchSlider, &QSlider::valueChanged,
            this, &TimeStretchDialog::onPitchSliderChanged);
    connect(m_pitchSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TimeStretchDialog::onPitchSpinChanged);
    connect(m_preservePitch, &QCheckBox::toggled,
            this, &TimeStretchDialog::onPreservePitchToggled);
    connect(m_previewBtn, &QPushButton::clicked,
            this, &TimeStretchDialog::onPreviewClicked);
    connect(m_applyBtn, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
}

// ── Slots ──────────────────────────────────────────────────────────────────────

void TimeStretchDialog::onStretchSliderChanged(int value)
{
    if (m_updatingControls) return;
    m_updatingControls = true;
    m_stretchSpin->setValue(static_cast<double>(value));
    updateDurationLabel();
    m_updatingControls = false;
}

void TimeStretchDialog::onStretchSpinChanged(double value)
{
    if (m_updatingControls) return;
    m_updatingControls = true;
    m_stretchSlider->setValue(static_cast<int>(value));
    updateDurationLabel();
    m_updatingControls = false;
}

void TimeStretchDialog::onPitchSliderChanged(int value)
{
    if (m_updatingControls) return;
    m_updatingControls = true;

    // Slider is in cents, spin is in semitones
    double semitones = static_cast<double>(value) / 100.0;
    m_pitchSpin->setValue(semitones);

    int cents = std::abs(value) % 100;
    int semi = std::abs(value) / 100;
    QString sign = (value >= 0) ? QStringLiteral("+") : QStringLiteral("-");
    if (value == 0) sign.clear();
    m_pitchLabel->setText(tr("%1%2 semitones, %3 cents").arg(sign).arg(semi).arg(cents));

    m_updatingControls = false;
}

void TimeStretchDialog::onPitchSpinChanged(double value)
{
    if (m_updatingControls) return;
    m_updatingControls = true;

    int cents = static_cast<int>(value * 100.0);
    m_pitchSlider->setValue(cents);

    int absCents = std::abs(cents) % 100;
    int semi = std::abs(cents) / 100;
    QString sign = (value >= 0.0) ? QStringLiteral("+") : QStringLiteral("-");
    if (value == 0.0) sign.clear();
    m_pitchLabel->setText(tr("%1%2 semitones, %3 cents").arg(sign).arg(semi).arg(absCents));

    m_updatingControls = false;
}

void TimeStretchDialog::onPreservePitchToggled(bool checked)
{
    // When preserve-pitch is off, disable the pitch slider (pitch follows speed)
    m_pitchSlider->setEnabled(checked);
    m_pitchSpin->setEnabled(checked);
    m_pitchLabel->setEnabled(checked);

    if (!checked) {
        // Show what the pitch would be based on stretch ratio
        float ratio = stretchRatio();
        float impliedSemitones = 12.0f * std::log2(ratio);
        m_pitchLabel->setText(tr("Pitch follows speed: %1%2 st")
            .arg(impliedSemitones >= 0.0f ? QStringLiteral("+") : QString())
            .arg(static_cast<double>(impliedSemitones), 0, 'f', 2));
    }
}

void TimeStretchDialog::onPreviewClicked()
{
    emit previewRequested(stretchRatio(), pitchSemitones(), preservePitch());
}

void TimeStretchDialog::updateDurationLabel()
{
    float ratio = stretchRatio();
    if (ratio > 0.0f) {
        float durationMultiplier = 1.0f / ratio;
        m_durationLabel->setText(tr("Duration: %1x").arg(
            static_cast<double>(durationMultiplier), 0, 'f', 2));
    }

    // If preserve-pitch is off, update the implied pitch label too
    if (!m_preservePitch->isChecked()) {
        onPreservePitchToggled(false);
    }
}

} // namespace dawcast::widgets
