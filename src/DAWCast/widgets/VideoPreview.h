// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QImage>

namespace dawcast::widgets {

class VideoPreview : public QWidget {
    Q_OBJECT

public:
    enum class ScaleMode { Fit, ActualSize, Half, Double };

    explicit VideoPreview(QWidget* parent = nullptr);
    ~VideoPreview() override;

    void setFrame(const QImage& frame);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QImage    m_frame;
    bool      m_playing   = false;
    ScaleMode m_scaleMode = ScaleMode::Fit;
};

} // namespace dawcast::widgets
