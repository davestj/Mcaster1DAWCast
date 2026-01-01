// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QImage>
#include <QList>
#include "../core/VideoFrame.h"

namespace dawcast {

class VideoMixer : public QObject
{
    Q_OBJECT

public:
    explicit VideoMixer(QObject* parent = nullptr);
    ~VideoMixer() override;

    /// Composite multiple video layers into a single output image.
    /// Handles PIP, overlays, and lower thirds.
    QImage composite(const QList<VideoFrame>& layers);

    void setOutputSize(int width, int height);
    [[nodiscard]] int outputWidth()  const { return m_outputWidth; }
    [[nodiscard]] int outputHeight() const { return m_outputHeight; }

private:
    int m_outputWidth  = 1920;
    int m_outputHeight = 1080;
};

} // namespace dawcast
