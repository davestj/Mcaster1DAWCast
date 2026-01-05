// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoEncoder.h"

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}
#endif

#include <QDebug>

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

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr,
                                              path.toUtf8().constData());
    if (ret < 0 || !fmtCtx) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoEncoder: avformat_alloc_output_context2 failed:" << errbuf;
        return false;
    }

    // Find the encoder
    const AVCodec* encoder = avcodec_find_encoder_by_name(codec.toUtf8().constData());
    if (!encoder) {
        qWarning() << "VideoEncoder: encoder not found:" << codec;
        avformat_free_context(fmtCtx);
        return false;
    }

    // Create a new video stream
    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        qWarning() << "VideoEncoder: avformat_new_stream failed";
        avformat_free_context(fmtCtx);
        return false;
    }

    // Allocate and configure codec context
    AVCodecContext* codecCtx = avcodec_alloc_context3(encoder);
    if (!codecCtx) {
        qWarning() << "VideoEncoder: avcodec_alloc_context3 failed";
        avformat_free_context(fmtCtx);
        return false;
    }

    codecCtx->width     = width;
    codecCtx->height    = height;
    codecCtx->pix_fmt   = AV_PIX_FMT_YUV420P;
    codecCtx->bit_rate  = 4000000;  // 4 Mbps default
    codecCtx->time_base = AVRational{1, static_cast<int>(fps * 1000)};
    stream->time_base   = codecCtx->time_base;
    codecCtx->framerate = AVRational{static_cast<int>(fps * 1000), 1000};
    codecCtx->gop_size  = 12;

    // If the output format requires global headers, set the flag
    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Set preset for libx264 / libx265
    if (codec == "libx264" || codec == "libx265") {
        av_opt_set(codecCtx->priv_data, "preset", "medium", 0);
    }

    ret = avcodec_open2(codecCtx, encoder, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoEncoder: avcodec_open2 failed:" << errbuf;
        avcodec_free_context(&codecCtx);
        avformat_free_context(fmtCtx);
        return false;
    }

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (ret < 0) {
        qWarning() << "VideoEncoder: avcodec_parameters_from_context failed";
        avcodec_free_context(&codecCtx);
        avformat_free_context(fmtCtx);
        return false;
    }

    // Initialize SwsContext for BGRA -> YUV420P conversion
    SwsContext* swsCtx = sws_getContext(
        width, height, AV_PIX_FMT_BGRA,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx) {
        qWarning() << "VideoEncoder: sws_getContext failed";
        avcodec_free_context(&codecCtx);
        avformat_free_context(fmtCtx);
        return false;
    }

    // Allocate reusable frame for YUV data
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        qWarning() << "VideoEncoder: av_frame_alloc failed";
        sws_freeContext(swsCtx);
        avcodec_free_context(&codecCtx);
        avformat_free_context(fmtCtx);
        return false;
    }
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width  = width;
    frame->height = height;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        qWarning() << "VideoEncoder: av_frame_get_buffer failed";
        av_frame_free(&frame);
        sws_freeContext(swsCtx);
        avcodec_free_context(&codecCtx);
        avformat_free_context(fmtCtx);
        return false;
    }

    // Allocate packet
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        qWarning() << "VideoEncoder: av_packet_alloc failed";
        av_frame_free(&frame);
        sws_freeContext(swsCtx);
        avcodec_free_context(&codecCtx);
        avformat_free_context(fmtCtx);
        return false;
    }

    // Open the output file
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmtCtx->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "VideoEncoder: avio_open failed:" << errbuf;
            av_packet_free(&pkt);
            av_frame_free(&frame);
            sws_freeContext(swsCtx);
            avcodec_free_context(&codecCtx);
            avformat_free_context(fmtCtx);
            return false;
        }
    }

    // Write the container header
    ret = avformat_write_header(fmtCtx, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoEncoder: avformat_write_header failed:" << errbuf;
        av_packet_free(&pkt);
        av_frame_free(&frame);
        sws_freeContext(swsCtx);
        avcodec_free_context(&codecCtx);
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        return false;
    }

    m_formatCtx = fmtCtx;
    m_codecCtx  = codecCtx;
    m_swsCtx    = swsCtx;
    m_frame     = frame;
    m_packet    = pkt;
    m_open = true;
    return true;

#else
    (void)path;
    (void)codec;
    qWarning() << "VideoEncoder: FFmpeg support not compiled in (HAVE_AVFORMAT not defined)";
    return false;
#endif
}

bool VideoEncoder::encodeFrame(const QImage& frame)
{
    if (!m_open) return false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx  = static_cast<AVFormatContext*>(m_formatCtx);
    AVCodecContext*  codecCtx = static_cast<AVCodecContext*>(m_codecCtx);
    SwsContext*      swsCtx  = static_cast<SwsContext*>(m_swsCtx);
    AVFrame*         avFrame = static_cast<AVFrame*>(m_frame);
    AVPacket*        pkt     = static_cast<AVPacket*>(m_packet);

    // Ensure the QImage is in ARGB32 (BGRA in memory) format
    QImage srcImage = frame;
    if (srcImage.width() != m_width || srcImage.height() != m_height) {
        srcImage = srcImage.scaled(m_width, m_height, Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);
    }
    if (srcImage.format() != QImage::Format_ARGB32)
        srcImage = srcImage.convertToFormat(QImage::Format_ARGB32);

    // Make the frame writable
    int ret = av_frame_make_writable(avFrame);
    if (ret < 0) {
        qWarning() << "VideoEncoder: av_frame_make_writable failed";
        return false;
    }

    // Convert BGRA -> YUV420P
    const uint8_t* srcData[4]     = { srcImage.constBits(), nullptr, nullptr, nullptr };
    int            srcLinesize[4] = { static_cast<int>(srcImage.bytesPerLine()), 0, 0, 0 };

    sws_scale(swsCtx,
              srcData, srcLinesize,
              0, m_height,
              avFrame->data, avFrame->linesize);

    avFrame->pts = m_frameIndex;

    // Send frame to encoder
    ret = avcodec_send_frame(codecCtx, avFrame);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "VideoEncoder: avcodec_send_frame failed:" << errbuf;
        return false;
    }

    // Receive and write all available encoded packets
    while (true) {
        ret = avcodec_receive_packet(codecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "VideoEncoder: avcodec_receive_packet failed:" << errbuf;
            return false;
        }

        // Rescale timestamps from codec time_base to stream time_base
        av_packet_rescale_ts(pkt, codecCtx->time_base,
                             fmtCtx->streams[0]->time_base);
        pkt->stream_index = 0;

        ret = av_interleaved_write_frame(fmtCtx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "VideoEncoder: av_interleaved_write_frame failed:" << errbuf;
            return false;
        }
    }

    ++m_frameIndex;
    emit progress(static_cast<int>(m_frameIndex));
    return true;

#else
    (void)frame;
    ++m_frameIndex;
    qWarning() << "VideoEncoder: FFmpeg support not compiled in";
    return false;
#endif
}

void VideoEncoder::close()
{
    if (!m_open) return;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx  = static_cast<AVFormatContext*>(m_formatCtx);
    AVCodecContext*  codecCtx = static_cast<AVCodecContext*>(m_codecCtx);
    AVPacket*        pkt     = static_cast<AVPacket*>(m_packet);

    // Flush the encoder by sending a null frame
    if (codecCtx) {
        avcodec_send_frame(codecCtx, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            av_packet_rescale_ts(pkt, codecCtx->time_base,
                                 fmtCtx->streams[0]->time_base);
            pkt->stream_index = 0;
            av_interleaved_write_frame(fmtCtx, pkt);
            av_packet_unref(pkt);
        }
    }

    // Write the container trailer
    if (fmtCtx)
        av_write_trailer(fmtCtx);

    // Free SwsContext
    if (m_swsCtx) {
        sws_freeContext(static_cast<SwsContext*>(m_swsCtx));
        m_swsCtx = nullptr;
    }

    // Free the reusable frame
    if (m_frame) {
        AVFrame* f = static_cast<AVFrame*>(m_frame);
        av_frame_free(&f);
        m_frame = nullptr;
    }

    // Free the reusable packet
    if (m_packet) {
        AVPacket* p = static_cast<AVPacket*>(m_packet);
        av_packet_free(&p);
        m_packet = nullptr;
    }

    // Free codec context
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        m_codecCtx = nullptr;
    }

    // Close file and free format context
    if (fmtCtx) {
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
    }
#endif

    m_formatCtx = nullptr;
    m_codecCtx  = nullptr;
    m_open = false;

    emit finished();
}

} // namespace dawcast
