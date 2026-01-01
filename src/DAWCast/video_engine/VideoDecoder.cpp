// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoDecoder.h"
// TODO: extern "C" {
// #include <libavformat/avformat.h>
// #include <libavcodec/avcodec.h>
// #include <libswscale/swscale.h>
// }

namespace dawcast {

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent)
{
}

VideoDecoder::~VideoDecoder()
{
    close();
}

bool VideoDecoder::open(const QString& path)
{
    if (m_open) close();

    // TODO: avformat_open_input()
    // TODO: avformat_find_stream_info()
    // TODO: Find video stream, open codec
    // TODO: Set m_width, m_height, m_fps from codec context

    (void)path;
    m_open = true;
    return true;
}

void VideoDecoder::close()
{
    if (!m_open) return;

    // TODO: avcodec_free_context()
    // TODO: avformat_close_input()
    m_formatCtx = nullptr;
    m_codecCtx  = nullptr;
    m_videoStreamIndex = -1;
    m_open = false;
}

void VideoDecoder::seekTo(int64_t pts)
{
    if (!m_open) return;
    // TODO: av_seek_frame(m_formatCtx, m_videoStreamIndex, pts, AVSEEK_FLAG_BACKWARD)
    // TODO: avcodec_flush_buffers()
    (void)pts;
}

bool VideoDecoder::decodeNextFrame()
{
    if (!m_open) return false;

    // TODO: av_read_frame() loop
    // TODO: avcodec_send_packet() / avcodec_receive_frame()
    // TODO: Convert AVFrame to QImage via sws_scale
    // TODO: Emit frameDecoded() with the VideoFrame
    // TODO: Return false and emit endOfStream() at EOF

    return false;
}

} // namespace dawcast
