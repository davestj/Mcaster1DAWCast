// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CrossfadeEditorDialog.h"
#include "../timeline/Clip.h"
#include "../timeline/CrossfadeCalc.h"
#include "../audio_engine/WaveformCache.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QDebug>

#include <cmath>
#include <algorithm>

namespace dawcast::widgets {

// ── Layout constants ────────────────────────────────────────────────────────

static constexpr int kWaveformWidth  = 500;
static constexpr int kWaveformHeight = 200;
static constexpr int kCurveWidth     = 500;
static constexpr int kCurveHeight    = 150;
static constexpr int kDialogMargin   = 16;

// ── Construction ────────────────────────────────────────────────────────────

CrossfadeEditorDialog::CrossfadeEditorDialog(Clip* clipA, Clip* clipB,
                                             QWidget* parent)
    : QDialog(parent)
    , m_clipA(clipA)
    , m_clipB(clipB)
{
    setWindowTitle(tr("Crossfade Editor"));

    // Compute the crossfade region from clip overlap
    if (m_clipA && m_clipB) {
        int64_t aEnd   = m_clipA->endPosition();
        int64_t bStart = m_clipB->timelinePosition();
        // If clips overlap: crossfade region = [bStart, aEnd)
        // If clips are adjacent: default 100ms crossfade centered on the boundary
        if (aEnd > bStart) {
            m_xfadeStart  = bStart;
            m_xfadeLength = aEnd - bStart;
        } else {
            // Adjacent or gap — default small crossfade at the boundary
            m_xfadeStart  = aEnd;
            m_xfadeLength = 4800;  // ~100ms at 48kHz
        }
    }

    setupUi();

    // Set initial display rects (below the controls at the top)
    m_waveformRect = QRect(kDialogMargin, kDialogMargin,
                           kWaveformWidth, kWaveformHeight);
    m_curveRect    = QRect(kDialogMargin,
                           kDialogMargin + kWaveformHeight + 8,
                           kCurveWidth, kCurveHeight);
}

CrossfadeEditorDialog::~CrossfadeEditorDialog() = default;

// ── Public accessors ────────────────────────────────────────────────────────

CrossfadeType CrossfadeEditorDialog::crossfadeType() const
{
    return static_cast<CrossfadeType>(m_typeCombo->currentIndex());
}

double CrossfadeEditorDialog::durationMs() const
{
    return m_durationSpin->value();
}

bool CrossfadeEditorDialog::isAsymmetric() const
{
    return m_asymmetricCheck->isChecked();
}

CrossfadeType CrossfadeEditorDialog::fadeOutType() const
{
    if (m_fadeOutTypeCombo)
        return static_cast<CrossfadeType>(m_fadeOutTypeCombo->currentIndex());
    return crossfadeType();
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void CrossfadeEditorDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(kDialogMargin, kDialogMargin,
                                   kDialogMargin, kDialogMargin);

    // ── Display area (custom painted) ──────────────────────────────────
    // Reserve space for the painted waveform + curve areas
    auto* displayWidget = new QWidget(this);
    displayWidget->setMinimumSize(kWaveformWidth,
                                  kWaveformHeight + kCurveHeight + 8);
    // We paint directly on the dialog, but this spacer ensures layout works
    mainLayout->addWidget(displayWidget);

    // ── Controls row ───────────────────────────────────────────────────
    auto* controlsGroup = new QGroupBox(tr("Crossfade Settings"), this);
    auto* controlsLayout = new QHBoxLayout(controlsGroup);

    // Type combo
    controlsLayout->addWidget(new QLabel(tr("Type:"), controlsGroup));
    m_typeCombo = new QComboBox(controlsGroup);
    m_typeCombo->addItem(tr("Equal Power"));
    m_typeCombo->addItem(tr("Linear"));
    m_typeCombo->addItem(tr("S-Curve"));
    m_typeCombo->addItem(tr("Logarithmic"));
    m_typeCombo->addItem(tr("Exponential"));
    controlsLayout->addWidget(m_typeCombo);

    controlsLayout->addSpacing(12);

    // Duration spinner
    controlsLayout->addWidget(new QLabel(tr("Duration (ms):"), controlsGroup));
    m_durationSpin = new QDoubleSpinBox(controlsGroup);
    m_durationSpin->setRange(10.0, 10000.0);
    m_durationSpin->setSingleStep(10.0);
    m_durationSpin->setDecimals(1);
    // Initialize from overlap length (assume 48kHz)
    double initMs = static_cast<double>(m_xfadeLength) / 48.0;
    m_durationSpin->setValue(std::clamp(initMs, 10.0, 10000.0));
    controlsLayout->addWidget(m_durationSpin);

    controlsLayout->addSpacing(12);

    // Asymmetric checkbox
    m_asymmetricCheck = new QCheckBox(tr("Asymmetric"), controlsGroup);
    controlsLayout->addWidget(m_asymmetricCheck);

    // Fade-out type combo (initially hidden, shown when asymmetric)
    m_fadeOutLabel = new QLabel(tr("Fade Out:"), controlsGroup);
    m_fadeOutLabel->setVisible(false);
    controlsLayout->addWidget(m_fadeOutLabel);

    m_fadeOutTypeCombo = new QComboBox(controlsGroup);
    m_fadeOutTypeCombo->addItem(tr("Equal Power"));
    m_fadeOutTypeCombo->addItem(tr("Linear"));
    m_fadeOutTypeCombo->addItem(tr("S-Curve"));
    m_fadeOutTypeCombo->addItem(tr("Logarithmic"));
    m_fadeOutTypeCombo->addItem(tr("Exponential"));
    m_fadeOutTypeCombo->setVisible(false);
    controlsLayout->addWidget(m_fadeOutTypeCombo);

    controlsLayout->addStretch();

    // Preview button
    m_previewBtn = new QPushButton(tr("Preview"), controlsGroup);
    controlsLayout->addWidget(m_previewBtn);

    mainLayout->addWidget(controlsGroup);

    // ── Preset buttons row ─────────────────────────────────────────────
    auto* presetsGroup = new QGroupBox(tr("Curve Presets"), this);
    auto* presetsLayout = new QHBoxLayout(presetsGroup);

    m_presetLinearBtn = new QPushButton(tr("Linear"), presetsGroup);
    m_presetEqualPowerBtn = new QPushButton(tr("Equal Power"), presetsGroup);
    m_presetFastFadeBtn = new QPushButton(tr("Fast Fade"), presetsGroup);
    m_presetSlowFadeBtn = new QPushButton(tr("Slow Fade"), presetsGroup);

    presetsLayout->addWidget(m_presetLinearBtn);
    presetsLayout->addWidget(m_presetEqualPowerBtn);
    presetsLayout->addWidget(m_presetFastFadeBtn);
    presetsLayout->addWidget(m_presetSlowFadeBtn);
    presetsLayout->addStretch();

    mainLayout->addWidget(presetsGroup);

    // ── OK / Cancel ────────────────────────────────────────────────────
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        applyCrossfade();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── Internal connections ───────────────────────────────────────────
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CrossfadeEditorDialog::onTypeChanged);
    connect(m_durationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CrossfadeEditorDialog::onDurationChanged);
    connect(m_asymmetricCheck, &QCheckBox::toggled,
            this, &CrossfadeEditorDialog::onAsymmetricToggled);
    connect(m_previewBtn, &QPushButton::clicked,
            this, &CrossfadeEditorDialog::onPreviewClicked);

    connect(m_presetLinearBtn, &QPushButton::clicked,
            this, &CrossfadeEditorDialog::onPresetLinear);
    connect(m_presetEqualPowerBtn, &QPushButton::clicked,
            this, &CrossfadeEditorDialog::onPresetEqualPower);
    connect(m_presetFastFadeBtn, &QPushButton::clicked,
            this, &CrossfadeEditorDialog::onPresetFastFade);
    connect(m_presetSlowFadeBtn, &QPushButton::clicked,
            this, &CrossfadeEditorDialog::onPresetSlowFade);

    // Also repaint when asymmetric fade-out type changes
    connect(m_fadeOutTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { update(); });

    setMinimumSize(kWaveformWidth + kDialogMargin * 2,
                   kWaveformHeight + kCurveHeight + 300);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void CrossfadeEditorDialog::onTypeChanged(int /*index*/)
{
    update();  // repaint curves
}

void CrossfadeEditorDialog::onDurationChanged(double ms)
{
    // Recalculate crossfade length in samples (assume 48kHz)
    m_xfadeLength = static_cast<int64_t>(ms * 48.0);
    update();
}

void CrossfadeEditorDialog::onAsymmetricToggled(bool checked)
{
    m_fadeOutLabel->setVisible(checked);
    m_fadeOutTypeCombo->setVisible(checked);
    update();
}

void CrossfadeEditorDialog::onPreviewClicked()
{
    // Preview playback of the crossfade region through PortAudio.
    // Full implementation would read audio from both clips, apply the
    // crossfade, and play through a temporary PortAudio stream. For now
    // this is a placeholder.
    qDebug() << "CrossfadeEditorDialog: preview requested"
             << "type =" << m_typeCombo->currentText()
             << "duration =" << m_durationSpin->value() << "ms";
}

void CrossfadeEditorDialog::onPresetLinear()
{
    m_typeCombo->setCurrentIndex(static_cast<int>(CrossfadeType::Linear));
    m_durationSpin->setValue(500.0);
}

void CrossfadeEditorDialog::onPresetEqualPower()
{
    m_typeCombo->setCurrentIndex(static_cast<int>(CrossfadeType::EqualPower));
    m_durationSpin->setValue(500.0);
}

void CrossfadeEditorDialog::onPresetFastFade()
{
    m_typeCombo->setCurrentIndex(static_cast<int>(CrossfadeType::Exponential));
    m_durationSpin->setValue(100.0);
}

void CrossfadeEditorDialog::onPresetSlowFade()
{
    m_typeCombo->setCurrentIndex(static_cast<int>(CrossfadeType::SCurve));
    m_durationSpin->setValue(2000.0);
}

// ── Curve evaluation ────────────────────────────────────────────────────────

float CrossfadeEditorDialog::evaluateCurve(CrossfadeType type, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type) {
    case CrossfadeType::EqualPower:
        return CrossfadeCalc::equalPower(t);
    case CrossfadeType::Linear:
        return CrossfadeCalc::linear(t);
    case CrossfadeType::SCurve:
        return CrossfadeCalc::sCurve(t);
    case CrossfadeType::Logarithmic:
        return CrossfadeCalc::logarithmic(t);
    case CrossfadeType::Exponential:
        return CrossfadeCalc::exponential(t);
    }
    return t;
}

// ── Apply crossfade to clips ────────────────────────────────────────────────

void CrossfadeEditorDialog::applyCrossfade()
{
    if (!m_clipA || !m_clipB) return;

    // Convert duration from ms to samples (assume 48kHz)
    int64_t fadeSamples = static_cast<int64_t>(m_durationSpin->value() * 48.0);

    // Apply fade-out to clip A and fade-in to clip B
    m_clipA->setFadeOut(fadeSamples);
    m_clipB->setFadeIn(fadeSamples);

    qDebug() << "CrossfadeEditorDialog: applied crossfade"
             << fadeSamples << "samples"
             << "type:" << m_typeCombo->currentText();
}

// ── Paint ───────────────────────────────────────────────────────────────────

void CrossfadeEditorDialog::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Update rects relative to current widget geometry
    m_waveformRect = QRect(kDialogMargin, kDialogMargin,
                           kWaveformWidth, kWaveformHeight);
    m_curveRect    = QRect(kDialogMargin,
                           kDialogMargin + kWaveformHeight + 8,
                           kCurveWidth, kCurveHeight);

    drawWaveformArea(painter, m_waveformRect);
    drawCurveArea(painter, m_curveRect);
}

// ── Waveform display ────────────────────────────────────────────────────────

void CrossfadeEditorDialog::drawWaveformArea(QPainter& painter, const QRect& rect)
{
    // Background
    painter.fillRect(rect, QColor(30, 30, 35));
    painter.setPen(QColor(60, 60, 70));
    painter.drawRect(rect);

    // Semi-transparent crossfade zone overlay
    painter.fillRect(rect, QColor(80, 120, 200, 30));

    const int w = rect.width();
    const int h = rect.height();
    const int midY = rect.top() + h / 2;

    // Try to draw actual waveform data from WaveformCache
    auto* cache = WaveformCache::instance();
    const CrossfadeType fadeInType  = crossfadeType();
    const CrossfadeType fadeOutType =
        isAsymmetric() ? this->fadeOutType() : fadeInType;

    // ── Clip A waveform (fading out) ───────────────────────────────────
    if (m_clipA) {
        const WaveformData* wfA = cache->getWaveform(m_clipA->sourcePath());
        if (wfA && !wfA->peaks.empty()) {
            painter.setPen(Qt::NoPen);

            int64_t clipStartInSource = m_xfadeStart - m_clipA->timelinePosition()
                                        + m_clipA->sourceIn();
            for (int px = 0; px < w; ++px) {
                float t = static_cast<float>(px) / static_cast<float>(w);
                float fadeGain = 1.0f - evaluateCurve(fadeOutType, t);

                // Map pixel to source sample position
                int64_t samplePos = clipStartInSource
                    + static_cast<int64_t>(t * static_cast<float>(m_xfadeLength));
                int blockIdx = static_cast<int>(samplePos / wfA->blockSize);
                if (blockIdx < 0 || blockIdx >= static_cast<int>(wfA->peaks.size()))
                    continue;

                float peak = wfA->peaks[static_cast<size_t>(blockIdx)] * fadeGain;
                int barH = static_cast<int>(peak * h * 0.45f);

                int alpha = static_cast<int>(fadeGain * 200.0f) + 40;
                painter.setBrush(QColor(100, 180, 255, alpha));
                painter.drawRect(rect.left() + px, midY - barH, 1, barH * 2);
            }
        } else {
            // Placeholder: draw fading-out gradient bar
            for (int px = 0; px < w; ++px) {
                float t = static_cast<float>(px) / static_cast<float>(w);
                float gain = 1.0f - evaluateCurve(fadeOutType, t);
                int alpha = static_cast<int>(gain * 180.0f) + 20;
                painter.setPen(QColor(100, 180, 255, alpha));
                int barH = static_cast<int>(gain * h * 0.35f);
                painter.drawLine(rect.left() + px, midY - barH,
                                 rect.left() + px, midY + barH);
            }
        }
    }

    // ── Clip B waveform (fading in) ────────────────────────────────────
    if (m_clipB) {
        const WaveformData* wfB = cache->getWaveform(m_clipB->sourcePath());
        if (wfB && !wfB->peaks.empty()) {
            painter.setPen(Qt::NoPen);

            int64_t clipStartInSource = m_xfadeStart - m_clipB->timelinePosition()
                                        + m_clipB->sourceIn();
            for (int px = 0; px < w; ++px) {
                float t = static_cast<float>(px) / static_cast<float>(w);
                float fadeGain = evaluateCurve(fadeInType, t);

                int64_t samplePos = clipStartInSource
                    + static_cast<int64_t>(t * static_cast<float>(m_xfadeLength));
                int blockIdx = static_cast<int>(samplePos / wfB->blockSize);
                if (blockIdx < 0 || blockIdx >= static_cast<int>(wfB->peaks.size()))
                    continue;

                float peak = wfB->peaks[static_cast<size_t>(blockIdx)] * fadeGain;
                int barH = static_cast<int>(peak * h * 0.45f);

                int alpha = static_cast<int>(fadeGain * 200.0f) + 40;
                painter.setBrush(QColor(255, 160, 80, alpha));
                painter.drawRect(rect.left() + px, midY - barH, 1, barH * 2);
            }
        } else {
            // Placeholder: draw fading-in gradient bar
            for (int px = 0; px < w; ++px) {
                float t = static_cast<float>(px) / static_cast<float>(w);
                float gain = evaluateCurve(fadeInType, t);
                int alpha = static_cast<int>(gain * 180.0f) + 20;
                painter.setPen(QColor(255, 160, 80, alpha));
                int barH = static_cast<int>(gain * h * 0.35f);
                painter.drawLine(rect.left() + px, midY - barH,
                                 rect.left() + px, midY + barH);
            }
        }
    }

    // Center line
    painter.setPen(QPen(QColor(255, 255, 255, 60), 1, Qt::DashLine));
    painter.drawLine(rect.left(), midY, rect.right(), midY);

    // Labels
    painter.setPen(QColor(200, 200, 200));
    QFont labelFont = font();
    labelFont.setPointSize(9);
    painter.setFont(labelFont);
    painter.drawText(rect.left() + 6, rect.top() + 16, tr("Clip A (out)"));
    painter.drawText(rect.right() - 80, rect.top() + 16, tr("Clip B (in)"));
}

// ── Curve display ───────────────────────────────────────────────────────────

void CrossfadeEditorDialog::drawCurveArea(QPainter& painter, const QRect& rect)
{
    // Background
    painter.fillRect(rect, QColor(25, 25, 30));
    painter.setPen(QColor(60, 60, 70));
    painter.drawRect(rect);

    const int w = rect.width();
    const int h = rect.height();
    const int margin = 4;

    // Grid lines (0.25, 0.5, 0.75)
    painter.setPen(QPen(QColor(60, 60, 80), 1, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        int y = rect.top() + margin + static_cast<int>((1.0f - i * 0.25f) * (h - margin * 2));
        painter.drawLine(rect.left() + margin, y, rect.right() - margin, y);
    }

    const CrossfadeType fadeInType  = crossfadeType();
    const CrossfadeType fadeOutType =
        isAsymmetric() ? this->fadeOutType() : fadeInType;

    // ── Fade-out curve (Clip A: 1.0 -> 0.0) ───────────────────────────
    {
        QPainterPath path;
        for (int px = 0; px < w - margin * 2; ++px) {
            float t = static_cast<float>(px) / static_cast<float>(w - margin * 2);
            float gain = 1.0f - evaluateCurve(fadeOutType, t);
            int x = rect.left() + margin + px;
            int y = rect.top() + margin
                    + static_cast<int>((1.0f - gain) * (h - margin * 2));
            if (px == 0) path.moveTo(x, y);
            else         path.lineTo(x, y);
        }
        painter.setPen(QPen(QColor(100, 180, 255), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    // ── Fade-in curve (Clip B: 0.0 -> 1.0) ────────────────────────────
    {
        QPainterPath path;
        for (int px = 0; px < w - margin * 2; ++px) {
            float t = static_cast<float>(px) / static_cast<float>(w - margin * 2);
            float gain = evaluateCurve(fadeInType, t);
            int x = rect.left() + margin + px;
            int y = rect.top() + margin
                    + static_cast<int>((1.0f - gain) * (h - margin * 2));
            if (px == 0) path.moveTo(x, y);
            else         path.lineTo(x, y);
        }
        painter.setPen(QPen(QColor(255, 160, 80), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    // Axis labels
    painter.setPen(QColor(160, 160, 160));
    QFont axisFont = font();
    axisFont.setPointSize(8);
    painter.setFont(axisFont);
    painter.drawText(rect.left() + 6, rect.bottom() - 4, tr("0"));
    painter.drawText(rect.right() - 28, rect.bottom() - 4, tr("Duration"));
    painter.drawText(rect.left() + 6, rect.top() + 12, tr("1.0"));
    painter.drawText(rect.left() + 6, rect.bottom() - 16, tr("0.0"));

    // Legend
    painter.setPen(QColor(100, 180, 255));
    painter.drawText(rect.right() - 130, rect.top() + 12, tr("Clip A (fade out)"));
    painter.setPen(QColor(255, 160, 80));
    painter.drawText(rect.right() - 130, rect.top() + 24, tr("Clip B (fade in)"));
}

} // namespace dawcast::widgets
