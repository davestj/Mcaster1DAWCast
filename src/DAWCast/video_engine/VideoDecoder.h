// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include "../core/VideoFrame.h"

namespace dawcast {

class VideoDecoder : public QObject
{
    Q_OBJECT

public:
    explicit VideoDecoder(QObject* parent = nullptr);
    ~VideoDecoder() override;

    bool open(const QString& path);
    void close();
    void seekTo(int64_t pts);
    bool decodeNextFrame();

    [[nodiscard]] bool isOpen() const { return m_open; }
    [[nodiscard]] int  width()  const { return m_width; }
    [[nodiscard]] int  height() const { return m_height; }
    [[nodiscard]] double fps()  const { return m_fps; }

signals:
    void frameDecoded(dawcast::VideoFrame frame);
    void endOfStream();

private:
    bool   m_open   = false;
    int    m_width  = 0;
    int    m_height = 0;
    double m_fps    = 0.0;

    // FFmpeg opaque pointers
    void* m_formatCtx = nullptr;  // AVFormatContext*
    void* m_codecCtx  = nullptr;  // AVCodecContext*
    void* m_swsCtx    = nullptr;  // SwsContext*
    void* m_frame     = nullptr;  // AVFrame*
    void* m_packet    = nullptr;  // AVPacket*
    int   m_videoStreamIndex = -1;
    int   m_audioStreamIndex = -1;
};

} // namespace dawcast
