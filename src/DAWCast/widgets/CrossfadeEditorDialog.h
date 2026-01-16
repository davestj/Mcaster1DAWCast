// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <cstdint>

namespace dawcast { class Clip; }
namespace dawcast { class WaveformData; }

namespace dawcast::widgets {

/// Crossfade curve type enumeration.
enum class CrossfadeType {
    EqualPower,
    Linear,
    SCurve,
    Logarithmic,
    Exponential
};

/// Visual crossfade editor dialog. Displays overlapping clip waveforms,
/// fade-in/fade-out curves, and lets the user choose curve type and duration.
class CrossfadeEditorDialog : public QDialog
{
    Q_OBJECT

public:
    CrossfadeEditorDialog(Clip* clipA, Clip* clipB, QWidget* parent = nullptr);
    ~CrossfadeEditorDialog() override;

    /// Currently selected crossfade type.
    [[nodiscard]] CrossfadeType crossfadeType() const;

    /// Crossfade duration in milliseconds.
    [[nodiscard]] double durationMs() const;

    /// Whether asymmetric mode (independent fade-in/out curves) is active.
    [[nodiscard]] bool isAsymmetric() const;

    /// Asymmetric fade-out type (only meaningful when asymmetric is checked).
    [[nodiscard]] CrossfadeType fadeOutType() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onTypeChanged(int index);
    void onDurationChanged(double ms);
    void onAsymmetricToggled(bool checked);
    void onPreviewClicked();
    void onPresetLinear();
    void onPresetEqualPower();
    void onPresetFastFade();
    void onPresetSlowFade();

private:
    // Layout helpers
    void setupUi();
    void applyCrossfade();

    // Curve evaluation: returns gain [0,1] for normalized t [0,1]
    static float evaluateCurve(CrossfadeType type, float t);

    // Draw the waveform display area
    void drawWaveformArea(QPainter& painter, const QRect& rect);

    // Draw the curve display area
    void drawCurveArea(QPainter& painter, const QRect& rect);

    // Clips being crossfaded
    Clip* m_clipA = nullptr;
    Clip* m_clipB = nullptr;

    // Crossfade region (computed from clip overlap)
    int64_t m_xfadeStart  = 0;   // timeline samples
    int64_t m_xfadeLength = 0;   // samples

    // Controls
    QComboBox*      m_typeCombo        = nullptr;
    QComboBox*      m_fadeOutTypeCombo = nullptr;   // shown only in asymmetric mode
    QLabel*         m_fadeOutLabel     = nullptr;
    QDoubleSpinBox* m_durationSpin     = nullptr;
    QCheckBox*      m_asymmetricCheck  = nullptr;
    QPushButton*    m_previewBtn       = nullptr;

    // Preset buttons
    QPushButton* m_presetLinearBtn     = nullptr;
    QPushButton* m_presetEqualPowerBtn = nullptr;
    QPushButton* m_presetFastFadeBtn   = nullptr;
    QPushButton* m_presetSlowFadeBtn   = nullptr;

    // Display areas (pixel rectangles, set during layout)
    QRect m_waveformRect;
    QRect m_curveRect;
};

} // namespace dawcast::widgets
