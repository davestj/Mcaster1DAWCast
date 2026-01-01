// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QImage>

namespace dawcast {

class VideoRenderer : public QWidget
{
    Q_OBJECT

public:
    explicit VideoRenderer(QWidget* parent = nullptr);
    ~VideoRenderer() override;

    void setFrame(const QImage& frame);
    [[nodiscard]] double aspectRatio() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_currentFrame;
};

} // namespace dawcast
