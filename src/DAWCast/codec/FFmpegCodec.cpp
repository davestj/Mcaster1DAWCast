// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FFmpegCodec.h"

#include <QDebug>

#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
}
#endif

namespace dawcast {

// ============================================================
// FFmpeg decode — generic audio file to float32 AudioBuffer
// ============================================================

AudioBuffer FFmpegCodec::decode(const QString &path)
{
#ifdef HAVE_AVFORMAT
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwrContext *swr = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    AudioBuffer result{};

    int ret = avformat_open_input(&fmtCtx, path.toUtf8().constData(),
                                  nullptr, nullptr);
    if (ret < 0) {
        qWarning() << "FFmpegCodec::decode: avformat_open_input() failed for"
                   << path;
        goto cleanup;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        qWarning() << "FFmpegCodec::decode: avformat_find_stream_info() failed";
        goto cleanup;
    }

    {
        // Find best audio stream
        int streamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO,
                                            -1, -1, nullptr, 0);
        if (streamIdx < 0) {
            qWarning() << "FFmpegCodec::decode: no audio stream found";
            goto cleanup;
        }

        AVStream *stream = fmtCtx->streams[streamIdx];
        const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            qWarning() << "FFmpegCodec::decode: no decoder for codec ID"
                       << stream->codecpar->codec_id;
            goto cleanup;
        }

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            qWarning() << "FFmpegCodec::decode: avcodec_alloc_context3() failed";
            goto cleanup;
        }

        ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::decode: avcodec_parameters_to_context() failed";
            goto cleanup;
        }

        ret = avcodec_open2(codecCtx, codec, nullptr);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::decode: avcodec_open2() failed";
            goto cleanup;
        }

        // Set up resampler to convert to interleaved float32
        int outChannels = codecCtx->ch_layout.nb_channels;
        int outSampleRate = codecCtx->sample_rate;

        ret = swr_alloc_set_opts2(&swr,
                                  &codecCtx->ch_layout,     // out ch layout
                                  AV_SAMPLE_FMT_FLT,        // out format (interleaved float)
                                  outSampleRate,             // out sample rate
                                  &codecCtx->ch_layout,     // in ch layout
                                  codecCtx->sample_fmt,      // in format
                                  codecCtx->sample_rate,     // in sample rate
                                  0, nullptr);
        if (ret < 0 || !swr) {
            qWarning() << "FFmpegCodec::decode: swr_alloc_set_opts2() failed";
            goto cleanup;
        }

        ret = swr_init(swr);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::decode: swr_init() failed";
            goto cleanup;
        }

        frame = av_frame_alloc();
        pkt = av_packet_alloc();
        if (!frame || !pkt) {
            qWarning() << "FFmpegCodec::decode: allocation failed";
            goto cleanup;
        }

        // Decode all frames
        std::vector<float> pcmData;

        while (av_read_frame(fmtCtx, pkt) >= 0) {
            if (pkt->stream_index != streamIdx) {
                av_packet_unref(pkt);
                continue;
            }

            ret = avcodec_send_packet(codecCtx, pkt);
            av_packet_unref(pkt);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) continue;
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) goto cleanup;

                // Convert this frame to float32
                int outSamples = swr_get_out_samples(swr, frame->nb_samples);
                std::vector<float> frameBuf(
                    static_cast<size_t>(outSamples * outChannels));
                uint8_t *outBuf = reinterpret_cast<uint8_t *>(frameBuf.data());

                int converted = swr_convert(swr,
                                            &outBuf, outSamples,
                                            const_cast<const uint8_t **>(frame->extended_data),
                                            frame->nb_samples);
                if (converted > 0) {
                    size_t count = static_cast<size_t>(converted * outChannels);
                    pcmData.insert(pcmData.end(),
                                   frameBuf.begin(),
                                   frameBuf.begin() + static_cast<ptrdiff_t>(count));
                }

                av_frame_unref(frame);
            }
        }

        // Flush decoder
        avcodec_send_packet(codecCtx, nullptr);
        while (true) {
            ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == AVERROR_EOF || ret < 0) break;

            int outSamples = swr_get_out_samples(swr, frame->nb_samples);
            std::vector<float> frameBuf(
                static_cast<size_t>(outSamples * outChannels));
            uint8_t *outBuf = reinterpret_cast<uint8_t *>(frameBuf.data());

            int converted = swr_convert(swr,
                                        &outBuf, outSamples,
                                        const_cast<const uint8_t **>(frame->extended_data),
                                        frame->nb_samples);
            if (converted > 0) {
                size_t count = static_cast<size_t>(converted * outChannels);
                pcmData.insert(pcmData.end(),
                               frameBuf.begin(),
                               frameBuf.begin() + static_cast<ptrdiff_t>(count));
            }
            av_frame_unref(frame);
        }

        // Flush resampler
        {
            int delayed = swr_get_out_samples(swr, 0);
            if (delayed > 0) {
                std::vector<float> flushBuf(
                    static_cast<size_t>(delayed * outChannels));
                uint8_t *outBuf = reinterpret_cast<uint8_t *>(flushBuf.data());
                int converted = swr_convert(swr, &outBuf, delayed,
                                            nullptr, 0);
                if (converted > 0) {
                    size_t count = static_cast<size_t>(converted * outChannels);
                    pcmData.insert(pcmData.end(),
                                   flushBuf.begin(),
                                   flushBuf.begin() + static_cast<ptrdiff_t>(count));
                }
            }
        }

        if (!pcmData.empty()) {
            int totalSamples = static_cast<int>(pcmData.size());
            int frames = totalSamples / outChannels;
            result.data = new float[static_cast<size_t>(totalSamples)];
            std::memcpy(result.data, pcmData.data(),
                        static_cast<size_t>(totalSamples) * sizeof(float));
            result.frames = frames;
            result.channels = outChannels;
            result.sampleRate = outSampleRate;
        }
    }

cleanup:
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (swr) swr_free(&swr);
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (fmtCtx) avformat_close_input(&fmtCtx);
    return result;

#else
    Q_UNUSED(path)
    qWarning() << "FFmpegCodec::decode: FFmpeg not available";
    return AudioBuffer{};
#endif
}

// ============================================================
// FFmpeg encode — float32 AudioBuffer to any supported format
// ============================================================

bool FFmpegCodec::encode(const AudioBuffer &buffer, const QString &path,
                         const QString &codecName, int bitrate)
{
#ifdef HAVE_AVFORMAT
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "FFmpegCodec::encode: invalid AudioBuffer";
        return false;
    }

    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwrContext *swr = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    bool success = false;

    // Create output format context (guess from file extension)
    int ret = avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr,
                                             path.toUtf8().constData());
    if (ret < 0 || !fmtCtx) {
        qWarning() << "FFmpegCodec::encode: avformat_alloc_output_context2() failed";
        goto cleanup;
    }

    {
        // Find encoder
        const AVCodec *codec = avcodec_find_encoder_by_name(
            codecName.toUtf8().constData());
        if (!codec) {
            qWarning() << "FFmpegCodec::encode: encoder not found:" << codecName;
            goto cleanup;
        }

        // Create new audio stream
        AVStream *stream = avformat_new_stream(fmtCtx, codec);
        if (!stream) {
            qWarning() << "FFmpegCodec::encode: avformat_new_stream() failed";
            goto cleanup;
        }

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            qWarning() << "FFmpegCodec::encode: avcodec_alloc_context3() failed";
            goto cleanup;
        }

        // Configure codec context
        if (bitrate > 0) {
            codecCtx->bit_rate = static_cast<int64_t>(bitrate) * 1000;
        }
        codecCtx->sample_rate = buffer.sampleRate;

        // Pick best sample format supported by encoder (prefer float)
        if (codec->sample_fmts) {
            codecCtx->sample_fmt = codec->sample_fmts[0];
            for (int i = 0; codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
                if (codec->sample_fmts[i] == AV_SAMPLE_FMT_FLTP ||
                    codec->sample_fmts[i] == AV_SAMPLE_FMT_FLT) {
                    codecCtx->sample_fmt = codec->sample_fmts[i];
                    break;
                }
            }
        } else {
            codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        }

        // Set channel layout
        av_channel_layout_default(&codecCtx->ch_layout, buffer.channels);

        codecCtx->time_base = AVRational{1, buffer.sampleRate};
        stream->time_base = codecCtx->time_base;

        if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
            codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ret = avcodec_open2(codecCtx, codec, nullptr);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::encode: avcodec_open2() failed";
            goto cleanup;
        }

        ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::encode: avcodec_parameters_from_context() failed";
            goto cleanup;
        }

        // Set up resampler: interleaved float32 -> encoder's format
        AVChannelLayout inLayout;
        av_channel_layout_default(&inLayout, buffer.channels);

        ret = swr_alloc_set_opts2(&swr,
                                  &codecCtx->ch_layout,
                                  codecCtx->sample_fmt,
                                  codecCtx->sample_rate,
                                  &inLayout,
                                  AV_SAMPLE_FMT_FLT,   // interleaved float input
                                  buffer.sampleRate,
                                  0, nullptr);
        av_channel_layout_uninit(&inLayout);

        if (ret < 0 || !swr) {
            qWarning() << "FFmpegCodec::encode: swr_alloc_set_opts2() failed";
            goto cleanup;
        }
        ret = swr_init(swr);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::encode: swr_init() failed";
            goto cleanup;
        }

        // Open output file
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&fmtCtx->pb, path.toUtf8().constData(),
                            AVIO_FLAG_WRITE);
            if (ret < 0) {
                qWarning() << "FFmpegCodec::encode: avio_open() failed";
                goto cleanup;
            }
        }

        ret = avformat_write_header(fmtCtx, nullptr);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::encode: avformat_write_header() failed";
            goto cleanup;
        }

        // Allocate frame and packet
        frame = av_frame_alloc();
        pkt = av_packet_alloc();
        if (!frame || !pkt) goto cleanup;

        int frameSize = codecCtx->frame_size;
        if (frameSize <= 0) frameSize = 1024; // fallback for variable-frame codecs

        frame->format = codecCtx->sample_fmt;
        av_channel_layout_copy(&frame->ch_layout, &codecCtx->ch_layout);
        frame->sample_rate = codecCtx->sample_rate;
        frame->nb_samples = frameSize;

        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            qWarning() << "FFmpegCodec::encode: av_frame_get_buffer() failed";
            goto cleanup;
        }

        // Encode loop
        int64_t pts = 0;
        int inputOffset = 0;
        int totalSamples = buffer.frames * buffer.channels;

        while (inputOffset < totalSamples) {
            int inputFramesLeft = (totalSamples - inputOffset) / buffer.channels;
            int framesToConvert = std::min(frameSize, inputFramesLeft);

            ret = av_frame_make_writable(frame);
            if (ret < 0) goto cleanup;

            frame->nb_samples = framesToConvert;

            // Convert from interleaved float to encoder format
            const uint8_t *inData = reinterpret_cast<const uint8_t *>(
                buffer.data + inputOffset);
            ret = swr_convert(swr,
                              frame->extended_data, framesToConvert,
                              &inData, framesToConvert);
            if (ret < 0) {
                qWarning() << "FFmpegCodec::encode: swr_convert() failed";
                goto cleanup;
            }
            frame->nb_samples = ret;
            frame->pts = pts;
            pts += ret;

            // Send frame to encoder
            ret = avcodec_send_frame(codecCtx, frame);
            if (ret < 0) {
                qWarning() << "FFmpegCodec::encode: avcodec_send_frame() failed";
                goto cleanup;
            }

            // Receive and write encoded packets
            while (ret >= 0) {
                ret = avcodec_receive_packet(codecCtx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) goto cleanup;

                pkt->stream_index = stream->index;
                av_packet_rescale_ts(pkt, codecCtx->time_base, stream->time_base);

                ret = av_interleaved_write_frame(fmtCtx, pkt);
                av_packet_unref(pkt);
                if (ret < 0) {
                    qWarning() << "FFmpegCodec::encode: av_interleaved_write_frame() failed";
                    goto cleanup;
                }
            }

            inputOffset += framesToConvert * buffer.channels;
        }

        // Flush encoder
        avcodec_send_frame(codecCtx, nullptr);
        while (true) {
            ret = avcodec_receive_packet(codecCtx, pkt);
            if (ret == AVERROR_EOF || ret < 0) break;

            pkt->stream_index = stream->index;
            av_packet_rescale_ts(pkt, codecCtx->time_base, stream->time_base);
            av_interleaved_write_frame(fmtCtx, pkt);
            av_packet_unref(pkt);
        }

        av_write_trailer(fmtCtx);
        success = true;
    }

cleanup:
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (swr) swr_free(&swr);
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (fmtCtx) {
        if (fmtCtx->pb && !(fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
    }
    return success;

#else
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(codecName)
    Q_UNUSED(bitrate)
    qWarning() << "FFmpegCodec::encode: FFmpeg not available";
    return false;
#endif
}

} // namespace dawcast
