// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoEncoder.h"
// TODO: extern "C" {
// #include <libavformat/avformat.h>
// #include <libavcodec/avcodec.h>
// #include <libswscale/swscale.h>
// }

namespace dawcast {

VideoEncoder::VideoEncoder(QObject* parent)
    : QObject(parent)
{
}

VideoEncoder::~VideoEncoder()
{
    close();
}

bool VideoEncoder::open(const QString& path, int width, int height, double fps,
                        const QString& codec)
{
    if (m_open) close();

    m_width  = width;
    m_height = height;
    m_fps    = fps;
    m_frameIndex = 0;

    // TODO: avformat_alloc_output_context2()
    // TODO: Find encoder by codec name
    // TODO: avcodec_open2()
    // TODO: avio_open()
    // TODO: avformat_write_header()

    (void)path;
    (void)codec;
    m_open = true;
    return true;
}

bool VideoEncoder::encodeFrame(const QImage& frame)
{
    if (!m_open) return false;

    // TODO: Convert QImage to AVFrame via sws_scale
    // TODO: avcodec_send_frame() / avcodec_receive_packet()
    // TODO: av_interleaved_write_frame()
    // TODO: Emit progress() based on frame count

    (void)frame;
    ++m_frameIndex;
    return true;
}

void VideoEncoder::close()
{
    if (!m_open) return;

    // TODO: Flush encoder
    // TODO: av_write_trailer()
    // TODO: avcodec_free_context()
    // TODO: avformat_free_context()

    m_formatCtx = nullptr;
    m_codecCtx  = nullptr;
    m_open = false;

    emit finished();
}

} // namespace dawcast
