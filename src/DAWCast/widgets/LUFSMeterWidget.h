// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

class QPainter;

namespace dawcast::widgets {

/// Real-time LUFS loudness meter per EBU R128.
///
/// Displays momentary (400ms), short-term (3s), integrated, loudness
/// range (LRA), and true-peak measurements in a compact vertical widget.
class LUFSMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit LUFSMeterWidget(QWidget* parent = nullptr);
    ~LUFSMeterWidget() override;

    // Called from audio engine with new measurement data
    void setMomentaryLUFS(float lufs);    // 400ms window
    void setShortTermLUFS(float lufs);    // 3s window
    void setIntegratedLUFS(float lufs);   // since play start
    void setLoudnessRange(float lu);      // LRA in LU
    void setTruePeak(float dbTP);         // true peak in dBTP

    void reset();  // Reset integrated measurement

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    float lufsToNormalized(float lufs) const;
    void  drawScaleMarkings(QPainter& p, const QRect& meterRect) const;
    void  drawTargetZone(QPainter& p, const QRect& meterRect) const;
    void  drawReadouts(QPainter& p, const QRect& area) const;
    QColor colorForLufs(float lufs) const;

    // Current measurement values
    float m_momentaryLUFS  = -60.0f;
    float m_shortTermLUFS  = -60.0f;
    float m_integratedLUFS = -60.0f;
    float m_loudnessRange  = 0.0f;
    float m_truePeak       = -60.0f;

    // Display state (smoothed for animation)
    float m_displayMomentary = -60.0f;
    float m_displayShortTerm = -60.0f;

    // Decay timer
    int m_decayTimer = 0;

    // Scale range
    static constexpr float kMinLUFS       = -60.0f;
    static constexpr float kMaxLUFS       =   6.0f;

    // Target zones (podcast: -16 LUFS, streaming: -14 LUFS)
    static constexpr float kTargetLow     = -16.0f;
    static constexpr float kTargetHigh    = -14.0f;

    // Layout constants
    static constexpr int kRefreshMs       = 30;     // ~33 fps
    static constexpr int kScaleWidth      = 30;     // width of LUFS scale markings
    static constexpr int kMeterBarWidth   = 18;     // width of the meter bar itself
    static constexpr int kReadoutHeight   = 60;     // height of numeric readouts area
    static constexpr int kLabelHeight     = 18;     // height of "LUFS" label at top
};

} // namespace dawcast::widgets
