// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QColor>

namespace dawcast::widgets {

class WaveformView : public QWidget {
    Q_OBJECT

public:
    explicit WaveformView(QWidget* parent = nullptr);
    ~WaveformView() override;

    void setWaveformData(const float* data, int frames);
    void setZoom(float zoom);
    void setColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const float* m_data   = nullptr;
    int          m_frames = 0;
    float        m_zoom   = 1.0f;
    QColor       m_color  = QColor(0, 180, 80);
};

} // namespace dawcast::widgets
