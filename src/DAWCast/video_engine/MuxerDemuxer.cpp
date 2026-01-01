// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MuxerDemuxer.h"
// TODO: extern "C" {
// #include <libavformat/avformat.h>
// #include <libavcodec/avcodec.h>
// }

namespace dawcast {

MuxerDemuxer::MuxerDemuxer(QObject* parent)
    : QObject(parent)
{
}

MuxerDemuxer::~MuxerDemuxer()
{
    close();
}

bool MuxerDemuxer::openInput(const QString& path)
{
    if (m_inputOpen) close();

    // TODO: avformat_open_input()
    // TODO: avformat_find_stream_info()
    (void)path;
    m_inputOpen = true;
    return true;
}

bool MuxerDemuxer::openOutput(const QString& path, const QString& format)
{
    if (m_outputOpen) close();

    // TODO: avformat_alloc_output_context2() with format hint
    // TODO: avio_open()
    (void)path;
    (void)format;
    m_outputOpen = true;
    return true;
}

bool MuxerDemuxer::addAudioStream(int sampleRate, int channels, const QString& codec)
{
    if (!m_outputOpen) return false;

    // TODO: avformat_new_stream()
    // TODO: Configure audio codec parameters
    (void)sampleRate;
    (void)channels;
    (void)codec;
    return true;
}

bool MuxerDemuxer::addVideoStream(int width, int height, double fps, const QString& codec)
{
    if (!m_outputOpen) return false;

    // TODO: avformat_new_stream()
    // TODO: Configure video codec parameters
    (void)width;
    (void)height;
    (void)fps;
    (void)codec;
    return true;
}

bool MuxerDemuxer::writeAudioPacket(const uint8_t* data, int size, int64_t pts)
{
    if (!m_outputOpen) return false;

    // TODO: Create AVPacket, set data/size/pts
    // TODO: av_interleaved_write_frame()
    (void)data;
    (void)size;
    (void)pts;
    return true;
}

bool MuxerDemuxer::writeVideoPacket(const uint8_t* data, int size, int64_t pts)
{
    if (!m_outputOpen) return false;

    // TODO: Create AVPacket, set data/size/pts
    // TODO: av_interleaved_write_frame()
    (void)data;
    (void)size;
    (void)pts;
    return true;
}

void MuxerDemuxer::close()
{
    if (m_outputOpen) {
        // TODO: av_write_trailer()
        // TODO: avio_closep()
        // TODO: avformat_free_context()
        m_outputFmtCtx = nullptr;
        m_outputOpen = false;
    }

    if (m_inputOpen) {
        // TODO: avformat_close_input()
        m_inputFmtCtx = nullptr;
        m_inputOpen = false;
    }

    m_audioStreamIdx = -1;
    m_videoStreamIdx = -1;
}

} // namespace dawcast
