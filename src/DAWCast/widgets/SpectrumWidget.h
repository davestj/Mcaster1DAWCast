// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <vector>

namespace dawcast::widgets {

enum class SpectrumMode {
    Bars,
    Line,
    Waterfall
};

class SpectrumWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    void processBuffer(const float* data, int frames);
    void setFFTSize(int size);
    void setDisplayMode(SpectrumMode mode);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int                m_fftSize = 2048;
    SpectrumMode       m_mode    = SpectrumMode::Bars;
    std::vector<float> m_magnitudes;
    std::vector<float> m_smoothed;
    std::vector<std::vector<float>> m_waterfallHistory;
};

} // namespace dawcast::widgets
