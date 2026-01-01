// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

class QPainter;

namespace dawcast::widgets {

class VUMeterWidget : public QWidget {
    Q_OBJECT

public:
    enum DisplayMode {
        Gradient,   // smooth gradient fill
        Segmented   // discrete LED-style segments
    };

    explicit VUMeterWidget(QWidget* parent = nullptr);
    ~VUMeterWidget() override;

    void setLevel(float peakDb, float rmsDb);
    void setPeakHoldTime(int ms);
    void setOrientation(Qt::Orientation orientation);
    void setDisplayMode(DisplayMode mode);
    void setMinDb(float db);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    float dbToNormalized(float db) const;
    void  drawScaleMarkings(QPainter& p, const QRect& meterRect) const;

    float           m_peakDb      = -60.0f;
    float           m_rmsDb       = -60.0f;
    float           m_peakHoldDb  = -60.0f;
    float           m_minDb       = -60.0f;
    int             m_peakHoldMs  = 1500;
    int             m_peakHoldCountdown = 0;
    int             m_decayTimer  = 0;
    Qt::Orientation m_orientation = Qt::Vertical;
    DisplayMode     m_displayMode = Gradient;

    static constexpr int kRefreshMs      = 30;    // ~33 fps
    static constexpr int kScaleWidth     = 26;    // width of dB scale markings
    static constexpr int kMeterBarWidth  = 14;    // width of the meter bar itself
};

} // namespace dawcast::widgets
