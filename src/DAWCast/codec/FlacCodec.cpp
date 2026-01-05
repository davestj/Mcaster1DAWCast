// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FlacCodec.h"
#include "FFmpegCodec.h"

#include <QDebug>

#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef HAVE_FLAC
#include <FLAC/stream_encoder.h>
#include <FLAC/stream_decoder.h>
#endif

namespace dawcast {

// ============================================================
// FLAC decode
// ============================================================

#ifdef HAVE_FLAC

// Callback context for the stream decoder
struct FlacDecodeContext {
    std::vector<float> samples;
    int channels   = 0;
    int sampleRate = 0;
    int totalFrames = 0;
    bool error     = false;
};

static FLAC__StreamDecoderWriteStatus flacWriteCb(
    const FLAC__StreamDecoder * /*decoder*/,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *clientData)
{
    auto *ctx = static_cast<FlacDecodeContext *>(clientData);
    int ch = static_cast<int>(frame->header.channels);
    int blocksize = static_cast<int>(frame->header.blocksize);
    int bps = static_cast<int>(frame->header.bits_per_sample);

    float scale = 1.0f / static_cast<float>(1 << (bps - 1));

    // Interleave channels
    for (int f = 0; f < blocksize; ++f) {
        for (int c = 0; c < ch; ++c) {
            ctx->samples.push_back(static_cast<float>(buffer[c][f]) * scale);
        }
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flacMetadataCb(
    const FLAC__StreamDecoder * /*decoder*/,
    const FLAC__StreamMetadata *metadata,
    void *clientData)
{
    auto *ctx = static_cast<FlacDecodeContext *>(clientData);
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        ctx->channels   = static_cast<int>(metadata->data.stream_info.channels);
        ctx->sampleRate = static_cast<int>(metadata->data.stream_info.sample_rate);
        ctx->totalFrames = static_cast<int>(metadata->data.stream_info.total_samples);
    }
}

static void flacErrorCb(
    const FLAC__StreamDecoder * /*decoder*/,
    FLAC__StreamDecoderErrorStatus status,
    void *clientData)
{
    auto *ctx = static_cast<FlacDecodeContext *>(clientData);
    ctx->error = true;
    qWarning() << "FlacCodec::decode: FLAC decoder error"
               << FLAC__StreamDecoderErrorStatusString[status];
}

#endif // HAVE_FLAC

AudioBuffer FlacCodec::decode(const QString &path)
{
#ifdef HAVE_FLAC
    FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
    if (!decoder) {
        qWarning() << "FlacCodec::decode: failed to create FLAC decoder";
        return AudioBuffer{};
    }

    FlacDecodeContext ctx;

    FLAC__StreamDecoderInitStatus initStatus =
        FLAC__stream_decoder_init_file(decoder,
                                       path.toUtf8().constData(),
                                       flacWriteCb,
                                       flacMetadataCb,
                                       flacErrorCb,
                                       &ctx);
    if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        qWarning() << "FlacCodec::decode: init failed —"
                   << FLAC__StreamDecoderInitStatusString[initStatus];
        FLAC__stream_decoder_delete(decoder);
        return AudioBuffer{};
    }

    bool ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);
    FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);

    if (!ok || ctx.error || ctx.channels <= 0 || ctx.sampleRate <= 0) {
        qWarning() << "FlacCodec::decode: decoding failed";
        return AudioBuffer{};
    }

    int totalSamples = static_cast<int>(ctx.samples.size());
    int frames = totalSamples / ctx.channels;

    AudioBuffer buf;
    buf.data = new float[static_cast<size_t>(totalSamples)];
    std::memcpy(buf.data, ctx.samples.data(),
                static_cast<size_t>(totalSamples) * sizeof(float));
    buf.frames = frames;
    buf.channels = ctx.channels;
    buf.sampleRate = ctx.sampleRate;
    return buf;

#else
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.decode(path);
#endif
}

// ============================================================
// FLAC encode
// ============================================================

bool FlacCodec::encode(const AudioBuffer &buffer, const QString &path, int bitDepth)
{
#ifdef HAVE_FLAC
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "FlacCodec::encode: invalid AudioBuffer";
        return false;
    }

    if (bitDepth != 16 && bitDepth != 24) {
        qWarning() << "FlacCodec::encode: unsupported bit depth" << bitDepth
                   << "— using 16";
        bitDepth = 16;
    }

    FLAC__StreamEncoder *encoder = FLAC__stream_encoder_new();
    if (!encoder) {
        qWarning() << "FlacCodec::encode: failed to create FLAC encoder";
        return false;
    }

    FLAC__stream_encoder_set_channels(encoder, static_cast<unsigned>(buffer.channels));
    FLAC__stream_encoder_set_bits_per_sample(encoder, static_cast<unsigned>(bitDepth));
    FLAC__stream_encoder_set_sample_rate(encoder, static_cast<unsigned>(buffer.sampleRate));
    FLAC__stream_encoder_set_compression_level(encoder, 5); // balanced speed/size
    FLAC__stream_encoder_set_verify(encoder, true);

    FLAC__StreamEncoderInitStatus initStatus =
        FLAC__stream_encoder_init_file(encoder,
                                       path.toUtf8().constData(),
                                       nullptr, // progress callback
                                       nullptr);
    if (initStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        qWarning() << "FlacCodec::encode: init failed —"
                   << FLAC__StreamEncoderInitStatusString[initStatus];
        FLAC__stream_encoder_delete(encoder);
        return false;
    }

    // Convert float32 to FLAC__int32
    int totalSamples = buffer.frames * buffer.channels;
    float scale = static_cast<float>((1 << (bitDepth - 1)) - 1);
    std::vector<FLAC__int32> intSamples(static_cast<size_t>(totalSamples));

    for (int i = 0; i < totalSamples; ++i) {
        float s = std::fmax(-1.0f, std::fmin(1.0f, buffer.data[i]));
        intSamples[static_cast<size_t>(i)] = static_cast<FLAC__int32>(
            std::lroundf(s * scale));
    }

    // FLAC expects non-interleaved per-channel pointers, but
    // FLAC__stream_encoder_process_interleaved() takes interleaved data
    FLAC__bool ok = FLAC__stream_encoder_process_interleaved(
        encoder,
        intSamples.data(),
        static_cast<unsigned>(buffer.frames));

    if (!ok) {
        qWarning() << "FlacCodec::encode: encoding failed —"
                   << FLAC__StreamEncoderStateString[
                          FLAC__stream_encoder_get_state(encoder)];
    }

    FLAC__stream_encoder_finish(encoder);
    FLAC__stream_encoder_delete(encoder);
    return ok != 0;

#else
    // No native FLAC support — attempt FFmpeg fallback
    Q_UNUSED(bitDepth)
    FFmpegCodec ffmpeg;
    return ffmpeg.encode(buffer, path, QStringLiteral("flac"), 0);
#endif
}

} // namespace dawcast
