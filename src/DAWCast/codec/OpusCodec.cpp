// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "OpusCodec.h"
#include "FFmpegCodec.h"

#include <QDebug>

#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef HAVE_OPUS
#include <opusenc.h>
#endif

#ifdef HAVE_OPUSFILE
#include <opusfile.h>
#endif

namespace dawcast {

// ============================================================
// Opus decode
// ============================================================

AudioBuffer OpusCodec::decode(const QString &path)
{
#ifdef HAVE_OPUSFILE
    int err = 0;
    OggOpusFile *of = op_open_file(path.toUtf8().constData(), &err);
    if (!of) {
        qWarning() << "OpusCodec::decode: op_open_file() failed, error" << err;
        // Fall through to FFmpeg fallback below
        goto ffmpeg_fallback;
    }

    {
        int channels = op_channel_count(of, -1);
        // Opus always decodes at 48000 Hz
        int sampleRate = 48000;
        ogg_int64_t totalPcm = op_pcm_total(of, -1);

        if (channels <= 0 || totalPcm <= 0) {
            qWarning() << "OpusCodec::decode: invalid stream info";
            op_free(of);
            goto ffmpeg_fallback;
        }

        int totalFrames = static_cast<int>(totalPcm);
        int totalSamples = totalFrames * channels;
        std::unique_ptr<float[]> pcmData(new float[static_cast<size_t>(totalSamples)]);

        int offset = 0;
        while (offset < totalSamples) {
            int remaining = totalSamples - offset;
            // op_read_float returns interleaved float samples
            int ret = op_read_float(of, pcmData.get() + offset, remaining, nullptr);
            if (ret < 0) {
                qWarning() << "OpusCodec::decode: op_read_float() error" << ret;
                op_free(of);
                goto ffmpeg_fallback;
            }
            if (ret == 0) break; // end of stream
            offset += ret * channels; // ret is in frames
        }

        op_free(of);

        int actualFrames = offset / channels;

        AudioBuffer buf;
        buf.data = pcmData.release();
        buf.frames = actualFrames;
        buf.channels = channels;
        buf.sampleRate = sampleRate;
        return buf;
    }

ffmpeg_fallback:
#endif
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.decode(path);
}

// ============================================================
// Opus encode
// ============================================================

bool OpusCodec::encode(const AudioBuffer &buffer, const QString &path, int bitrate)
{
#ifdef HAVE_OPUS
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "OpusCodec::encode: invalid AudioBuffer";
        return false;
    }
    if (buffer.channels > 2) {
        qWarning() << "OpusCodec::encode: only mono/stereo supported, got"
                   << buffer.channels;
        return false;
    }

    // libopusenc expects 48000 Hz input. If we have a different rate,
    // it will resample internally, but we set the original rate as metadata.
    OggOpusComments *comments = ope_comments_create();
    if (!comments) {
        qWarning() << "OpusCodec::encode: ope_comments_create() failed";
        return false;
    }
    ope_comments_add(comments, "ENCODER", "Mcaster1DAWCast");

    int err = OPE_OK;
    OggOpusEnc *enc = ope_encoder_create_file(
        path.toUtf8().constData(),
        comments,
        buffer.sampleRate,
        buffer.channels,
        buffer.channels > 1 ? 1 : 0, // family: 0=mono/stereo, 1=surround mapping
        &err);

    if (!enc || err != OPE_OK) {
        qWarning() << "OpusCodec::encode: ope_encoder_create_file() failed, error" << err;
        ope_comments_destroy(comments);
        return false;
    }

    // Set bitrate (libopusenc uses bits/second)
    ope_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate * 1000));

    // Write all samples at once (libopusenc handles framing internally)
    err = ope_encoder_write_float(enc, buffer.data, buffer.frames);
    if (err != OPE_OK) {
        qWarning() << "OpusCodec::encode: ope_encoder_write_float() failed, error" << err;
        ope_encoder_drain(enc);
        ope_encoder_destroy(enc);
        ope_comments_destroy(comments);
        return false;
    }

    // Drain and finalize
    err = ope_encoder_drain(enc);
    if (err != OPE_OK) {
        qWarning() << "OpusCodec::encode: ope_encoder_drain() failed, error" << err;
    }

    ope_encoder_destroy(enc);
    ope_comments_destroy(comments);
    return err == OPE_OK;

#else
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.encode(buffer, path, QStringLiteral("libopus"), bitrate);
#endif
}

} // namespace dawcast
