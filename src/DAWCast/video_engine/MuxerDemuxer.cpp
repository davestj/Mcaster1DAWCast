// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MuxerDemuxer.h"

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#endif

#include <QDebug>

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

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "MuxerDemuxer: avformat_open_input failed:" << errbuf;
        return false;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "MuxerDemuxer: avformat_find_stream_info failed:" << errbuf;
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Enumerate streams and store indices
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        AVStream* s = fmtCtx->streams[i];
        if (s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_audioStreamIdx < 0)
            m_audioStreamIdx = static_cast<int>(i);
        else if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_videoStreamIdx < 0)
            m_videoStreamIdx = static_cast<int>(i);
    }

    m_inputFmtCtx = fmtCtx;
    m_inputOpen = true;
    return true;

#else
    (void)path;
    qWarning() << "MuxerDemuxer: FFmpeg support not compiled in (HAVE_AVFORMAT not defined)";
    return false;
#endif
}

bool MuxerDemuxer::openOutput(const QString& path, const QString& format)
{
    if (m_outputOpen) close();

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_alloc_output_context2(
        &fmtCtx, nullptr,
        format.toUtf8().constData(),
        path.toUtf8().constData());
    if (ret < 0 || !fmtCtx) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "MuxerDemuxer: avformat_alloc_output_context2 failed:" << errbuf;
        return false;
    }

    // Open the output file (unless the format is a special "no-file" type)
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmtCtx->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "MuxerDemuxer: avio_open failed:" << errbuf;
            avformat_free_context(fmtCtx);
            return false;
        }
    }

    m_outputFmtCtx = fmtCtx;
    m_outputOpen = true;
    m_headerWritten = false;
    return true;

#else
    (void)path;
    (void)format;
    qWarning() << "MuxerDemuxer: FFmpeg support not compiled in (HAVE_AVFORMAT not defined)";
    return false;
#endif
}

bool MuxerDemuxer::addAudioStream(int sampleRate, int channels, const QString& codec)
{
    if (!m_outputOpen) return false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = static_cast<AVFormatContext*>(m_outputFmtCtx);

    const AVCodec* encoder = avcodec_find_encoder_by_name(codec.toUtf8().constData());
    if (!encoder) {
        qWarning() << "MuxerDemuxer: audio encoder not found:" << codec;
        return false;
    }

    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        qWarning() << "MuxerDemuxer: avformat_new_stream (audio) failed";
        return false;
    }

    // Configure codec parameters on the stream
    stream->codecpar->codec_type  = AVMEDIA_TYPE_AUDIO;
    stream->codecpar->codec_id    = encoder->id;
    stream->codecpar->sample_rate = sampleRate;
    stream->codecpar->ch_layout.nb_channels = channels;
    av_channel_layout_default(&stream->codecpar->ch_layout, channels);

    // Use the encoder's preferred sample format.
    // PCM codecs need interleaved float (FLT), while AAC/MP3 prefer planar (FLTP).
    if (encoder->sample_fmts) {
        stream->codecpar->format = encoder->sample_fmts[0];
    } else {
        stream->codecpar->format = AV_SAMPLE_FMT_FLTP;
    }
    stream->time_base             = AVRational{1, sampleRate};

    m_audioStreamIdx = static_cast<int>(fmtCtx->nb_streams - 1);
    return true;

#else
    (void)sampleRate;
    (void)channels;
    (void)codec;
    return false;
#endif
}

bool MuxerDemuxer::addVideoStream(int width, int height, double fps, const QString& codec)
{
    if (!m_outputOpen) return false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = static_cast<AVFormatContext*>(m_outputFmtCtx);

    const AVCodec* encoder = avcodec_find_encoder_by_name(codec.toUtf8().constData());
    if (!encoder) {
        qWarning() << "MuxerDemuxer: video encoder not found:" << codec;
        return false;
    }

    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        qWarning() << "MuxerDemuxer: avformat_new_stream (video) failed";
        return false;
    }

    // Configure codec parameters on the stream
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id   = encoder->id;
    stream->codecpar->width      = width;
    stream->codecpar->height     = height;
    stream->codecpar->format     = AV_PIX_FMT_YUV420P;

    int fpsInt = static_cast<int>(fps * 1000);
    stream->time_base        = AVRational{1, fpsInt};
    stream->avg_frame_rate   = AVRational{fpsInt, 1000};
    stream->r_frame_rate     = AVRational{fpsInt, 1000};

    m_videoStreamIdx = static_cast<int>(fmtCtx->nb_streams - 1);
    return true;

#else
    (void)width;
    (void)height;
    (void)fps;
    (void)codec;
    return false;
#endif
}

bool MuxerDemuxer::writeAudioPacket(const uint8_t* data, int size, int64_t pts)
{
    if (!m_outputOpen) return false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = static_cast<AVFormatContext*>(m_outputFmtCtx);

    // Write the header on first packet if not yet written
    if (!m_headerWritten) {
        int ret = avformat_write_header(fmtCtx, nullptr);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "MuxerDemuxer: avformat_write_header failed:" << errbuf;
            return false;
        }
        m_headerWritten = true;
    }

    if (m_audioStreamIdx < 0) {
        qWarning() << "MuxerDemuxer: no audio stream configured";
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        qWarning() << "MuxerDemuxer: av_packet_alloc failed";
        return false;
    }

    // Wrap the caller's data buffer (no copy -- packet does not own the buffer)
    pkt->data         = const_cast<uint8_t*>(data);
    pkt->size         = size;
    pkt->pts          = pts;
    pkt->dts          = pts;
    pkt->stream_index = m_audioStreamIdx;

    int ret = av_interleaved_write_frame(fmtCtx, pkt);
    av_packet_free(&pkt);

    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "MuxerDemuxer: writeAudioPacket failed:" << errbuf;
        return false;
    }
    return true;

#else
    (void)data;
    (void)size;
    (void)pts;
    return false;
#endif
}

bool MuxerDemuxer::writeVideoPacket(const uint8_t* data, int size, int64_t pts)
{
    if (!m_outputOpen) return false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = static_cast<AVFormatContext*>(m_outputFmtCtx);

    // Write the header on first packet if not yet written
    if (!m_headerWritten) {
        int ret = avformat_write_header(fmtCtx, nullptr);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "MuxerDemuxer: avformat_write_header failed:" << errbuf;
            return false;
        }
        m_headerWritten = true;
    }

    if (m_videoStreamIdx < 0) {
        qWarning() << "MuxerDemuxer: no video stream configured";
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        qWarning() << "MuxerDemuxer: av_packet_alloc failed";
        return false;
    }

    pkt->data         = const_cast<uint8_t*>(data);
    pkt->size         = size;
    pkt->pts          = pts;
    pkt->dts          = pts;
    pkt->stream_index = m_videoStreamIdx;

    int ret = av_interleaved_write_frame(fmtCtx, pkt);
    av_packet_free(&pkt);

    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "MuxerDemuxer: writeVideoPacket failed:" << errbuf;
        return false;
    }
    return true;

#else
    (void)data;
    (void)size;
    (void)pts;
    return false;
#endif
}

void MuxerDemuxer::close()
{
    if (m_outputOpen) {
#ifdef HAVE_AVFORMAT
        AVFormatContext* fmtCtx = static_cast<AVFormatContext*>(m_outputFmtCtx);
        if (fmtCtx) {
            if (m_headerWritten)
                av_write_trailer(fmtCtx);
            if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
                avio_closep(&fmtCtx->pb);
            avformat_free_context(fmtCtx);
        }
#endif
        m_outputFmtCtx = nullptr;
        m_outputOpen = false;
        m_headerWritten = false;
    }

    if (m_inputOpen) {
#ifdef HAVE_AVFORMAT
        AVFormatContext* fmtCtx = static_cast<AVFormatContext*>(m_inputFmtCtx);
        if (fmtCtx)
            avformat_close_input(&fmtCtx);
#endif
        m_inputFmtCtx = nullptr;
        m_inputOpen = false;
    }

    m_audioStreamIdx = -1;
    m_videoStreamIdx = -1;
}

} // namespace dawcast
