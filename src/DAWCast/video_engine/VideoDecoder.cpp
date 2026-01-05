// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoDecoder.h"

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

#include <QDebug>

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

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoDecoder: avformat_open_input failed:" << errbuf;
        return false;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoDecoder: avformat_find_stream_info failed:" << errbuf;
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Find the best video stream
    int videoIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIdx < 0) {
        qWarning() << "VideoDecoder: no video stream found";
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVStream* videoStream = fmtCtx->streams[videoIdx];
    const AVCodec* decoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!decoder) {
        qWarning() << "VideoDecoder: unsupported video codec";
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        qWarning() << "VideoDecoder: avcodec_alloc_context3 failed";
        avformat_close_input(&fmtCtx);
        return false;
    }

    ret = avcodec_parameters_to_context(codecCtx, videoStream->codecpar);
    if (ret < 0) {
        qWarning() << "VideoDecoder: avcodec_parameters_to_context failed";
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    ret = avcodec_open2(codecCtx, decoder, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoDecoder: avcodec_open2 failed:" << errbuf;
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Initialize the scaler: source format -> BGRA for QImage
    SwsContext* swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx) {
        qWarning() << "VideoDecoder: sws_getContext failed";
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Also find audio stream (optional, for future use)
    int audioIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    // Store all contexts
    m_formatCtx = fmtCtx;
    m_codecCtx  = codecCtx;
    m_swsCtx    = swsCtx;
    m_videoStreamIndex = videoIdx;
    m_audioStreamIndex = audioIdx;

    // Allocate reusable frame and packet
    m_frame  = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet) {
        qWarning() << "VideoDecoder: failed to allocate frame/packet";
        close();
        return false;
    }

    m_width  = codecCtx->width;
    m_height = codecCtx->height;

    // Compute fps from stream time_base and avg_frame_rate
    if (videoStream->avg_frame_rate.den > 0 && videoStream->avg_frame_rate.num > 0) {
        m_fps = av_q2d(videoStream->avg_frame_rate);
    } else if (videoStream->r_frame_rate.den > 0 && videoStream->r_frame_rate.num > 0) {
        m_fps = av_q2d(videoStream->r_frame_rate);
    } else {
        m_fps = 25.0;
    }

    m_open = true;
    return true;

#else
    (void)path;
    qWarning() << "VideoDecoder: FFmpeg support not compiled in (HAVE_AVFORMAT not defined)";
    return false;
#endif
}

void VideoDecoder::close()
{
    if (!m_open) return;

#ifdef HAVE_AVFORMAT
    if (m_swsCtx) {
        sws_freeContext(static_cast<SwsContext*>(m_swsCtx));
        m_swsCtx = nullptr;
    }

    if (m_frame) {
        av_frame_free(reinterpret_cast<AVFrame**>(&m_frame));
        m_frame = nullptr;
    }

    if (m_packet) {
        av_packet_free(reinterpret_cast<AVPacket**>(&m_packet));
        m_packet = nullptr;
    }

    if (m_codecCtx) {
        AVCodecContext* ctx = static_cast<AVCodecContext*>(m_codecCtx);
        avcodec_free_context(&ctx);
        m_codecCtx = nullptr;
    }

    if (m_formatCtx) {
        AVFormatContext* ctx = static_cast<AVFormatContext*>(m_formatCtx);
        avformat_close_input(&ctx);
        m_formatCtx = nullptr;
    }
#endif

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_width  = 0;
    m_height = 0;
    m_fps    = 0.0;
    m_open   = false;
}

void VideoDecoder::seekTo(int64_t pts)
{
    if (!m_open) return;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx  = static_cast<AVFormatContext*>(m_formatCtx);
    AVCodecContext*  codecCtx = static_cast<AVCodecContext*>(m_codecCtx);

    int ret = av_seek_frame(fmtCtx, m_videoStreamIndex, pts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoDecoder: av_seek_frame failed:" << errbuf;
        return;
    }

    avcodec_flush_buffers(codecCtx);
#else
    (void)pts;
#endif
}

bool VideoDecoder::decodeNextFrame()
{
    if (!m_open) return false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx  = static_cast<AVFormatContext*>(m_formatCtx);
    AVCodecContext*  codecCtx = static_cast<AVCodecContext*>(m_codecCtx);
    SwsContext*      swsCtx  = static_cast<SwsContext*>(m_swsCtx);
    AVFrame*         frame   = static_cast<AVFrame*>(m_frame);
    AVPacket*        pkt     = static_cast<AVPacket*>(m_packet);

    while (true) {
        int ret = av_read_frame(fmtCtx, pkt);
        if (ret < 0) {
            // End of stream or error
            if (ret == AVERROR_EOF || avio_feof(fmtCtx->pb)) {
                emit endOfStream();
            } else {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                qWarning() << "VideoDecoder: av_read_frame error:" << errbuf;
            }
            return false;
        }

        // Skip non-video packets
        if (pkt->stream_index != m_videoStreamIndex) {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(codecCtx, pkt);
        av_packet_unref(pkt);

        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "VideoDecoder: avcodec_send_packet failed:" << errbuf;
            return false;
        }

        ret = avcodec_receive_frame(codecCtx, frame);
        if (ret == AVERROR(EAGAIN)) {
            // Need more packets before we get a frame
            continue;
        }
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                emit endOfStream();
            } else {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                qWarning() << "VideoDecoder: avcodec_receive_frame failed:" << errbuf;
            }
            return false;
        }

        // Convert decoded frame to BGRA QImage
        QImage img(frame->width, frame->height, QImage::Format_ARGB32);
        uint8_t* dstData[4]     = { img.bits(), nullptr, nullptr, nullptr };
        int      dstLinesize[4] = { static_cast<int>(img.bytesPerLine()), 0, 0, 0 };

        sws_scale(swsCtx,
                  frame->data, frame->linesize,
                  0, frame->height,
                  dstData, dstLinesize);

        // Compute presentation time in seconds
        AVStream* videoStream = fmtCtx->streams[m_videoStreamIndex];
        double timeSeconds = 0.0;
        if (frame->pts != AV_NOPTS_VALUE) {
            timeSeconds = frame->pts * av_q2d(videoStream->time_base);
        }

        VideoFrame vf;
        vf.image       = img;
        vf.pts         = frame->pts;
        vf.width       = frame->width;
        vf.height      = frame->height;
        vf.timeSeconds = timeSeconds;

        av_frame_unref(frame);
        emit frameDecoded(vf);
        return true;
    }

#else
    qWarning() << "VideoDecoder: FFmpeg support not compiled in";
    return false;
#endif
}

} // namespace dawcast
