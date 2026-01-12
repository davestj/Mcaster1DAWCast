// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QColor>
#include <array>

class QComboBox;
class QCheckBox;

namespace dawcast { class ParametricEQ; }

namespace dawcast::widgets {

class EmbossedKnob;

/// Interactive visual editor dialog for the 10-band Parametric EQ.
///
/// The upper portion displays a logarithmic frequency-response curve (20 Hz to
/// 20 kHz) with draggable band control points.  The lower portion provides
/// per-band controls (type, frequency, gain, Q, enable/bypass).
///
/// Mouse interaction on the curve:
///   - Click a band point to select it
///   - Drag horizontally to change frequency
///   - Drag vertically to change gain
///   - Mouse wheel to change Q
///   - Double-click a point to enable/disable the band

class ParametricEQDialog : public QDialog {
    Q_OBJECT

public:
    explicit ParametricEQDialog(ParametricEQ* eq, QWidget* parent = nullptr);
    ~ParametricEQDialog() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // Coordinate mapping helpers
    int   freqToX(float freq) const;
    float xToFreq(int x) const;
    int   dbToY(float db) const;
    float yToDb(int y) const;

    // Biquad magnitude response at a given frequency
    float bandMagnitudeDb(int band, float freq) const;
    float compositeMagnitudeDb(float freq) const;

    // Synchronise dialog controls with the ParametricEQ model
    void syncControlsFromEQ();
    void pushBandToEQ(int band);

    // Hit-test: returns band index at (x,y) or -1
    int hitTestBand(int x, int y) const;

    // Layout constants
    QRect curveRect() const;

    ParametricEQ* m_eq = nullptr;
    float m_sampleRate = 48000.0f;

    static constexpr int NumBands = 10;

    // Per-band UI state
    struct BandUI {
        float freq   = 1000.0f;
        float gainDb = 0.0f;
        float q      = 0.707f;
        int   type   = 1; // 0=LS, 1=PK, 2=HS, 3=HP, 4=LP
        bool  enabled = true;
    };
    std::array<BandUI, NumBands> m_bands;

    // Per-band widgets
    struct BandWidgets {
        QComboBox*   typeCombo  = nullptr;
        EmbossedKnob* freqKnob = nullptr;
        EmbossedKnob* gainKnob = nullptr;
        EmbossedKnob* qKnob    = nullptr;
        QCheckBox*   enableCB  = nullptr;
    };
    std::array<BandWidgets, NumBands> m_widgets;

    int  m_selectedBand  = -1;
    int  m_draggingBand  = -1;
    bool m_dragging      = false;

    // Band colors (unique per band)
    static const QColor kBandColors[NumBands];

    // Curve display area dimensions
    static constexpr int kCurveLeft   = 50;
    static constexpr int kCurveTop    = 20;
    static constexpr int kCurveWidth  = 600;
    static constexpr int kCurveHeight = 300;
    static constexpr int kControlsTop = kCurveTop + kCurveHeight + 20;

    // Frequency range
    static constexpr float kMinFreq = 20.0f;
    static constexpr float kMaxFreq = 20000.0f;

    // dB range
    static constexpr float kMinDb = -24.0f;
    static constexpr float kMaxDb =  24.0f;

    // Point radius
    static constexpr int kPointRadius    = 7;
    static constexpr int kPointHitRadius = 12;
};

} // namespace dawcast::widgets
