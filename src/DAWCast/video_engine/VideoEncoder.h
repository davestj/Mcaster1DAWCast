// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QImage>
#include <QString>

namespace dawcast {

class VideoEncoder : public QObject
{
    Q_OBJECT

public:
    explicit VideoEncoder(QObject* parent = nullptr);
    ~VideoEncoder() override;

    bool open(const QString& path, int width, int height, double fps,
              const QString& codec = "libx264");
    bool encodeFrame(const QImage& frame);
    void close();

    [[nodiscard]] bool isOpen() const { return m_open; }

signals:
    void progress(int percent);
    void finished();

private:
    bool   m_open   = false;
    int    m_width  = 0;
    int    m_height = 0;
    double m_fps    = 0.0;

    // FFmpeg opaque pointers
    void* m_formatCtx = nullptr;  // AVFormatContext*
    void* m_codecCtx  = nullptr;  // AVCodecContext*
    int64_t m_frameIndex = 0;
};

} // namespace dawcast
