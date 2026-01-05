// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Mp3Codec.h"
#include "FFmpegCodec.h"

#include <QDebug>
#include <QFile>

#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef HAVE_LAME
#include <lame/lame.h>
#endif

#ifdef HAVE_MPG123
#include <mpg123.h>
#endif

namespace dawcast {

// ============================================================
// MP3 decode
// ============================================================

AudioBuffer Mp3Codec::decode(const QString &path)
{
#ifdef HAVE_MPG123
    int err = MPG123_OK;

    // mpg123 requires a one-time library init (safe to call multiple times)
    static bool mpg123Initialized = false;
    if (!mpg123Initialized) {
        if (mpg123_init() != MPG123_OK) {
            qWarning() << "Mp3Codec::decode: mpg123_init() failed";
            return AudioBuffer{};
        }
        mpg123Initialized = true;
    }

    mpg123_handle *mh = mpg123_new(nullptr, &err);
    if (!mh) {
        qWarning() << "Mp3Codec::decode: mpg123_new() failed —"
                   << mpg123_plain_strerror(err);
        return AudioBuffer{};
    }

    // Request float output
    mpg123_param(mh, MPG123_FLAGS, MPG123_FORCE_FLOAT, 0.0);

    if (mpg123_open(mh, path.toUtf8().constData()) != MPG123_OK) {
        qWarning() << "Mp3Codec::decode: cannot open" << path
                   << "—" << mpg123_strerror(mh);
        mpg123_delete(mh);
        return AudioBuffer{};
    }

    long rate = 0;
    int channels = 0;
    int encoding = 0;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        qWarning() << "Mp3Codec::decode: mpg123_getformat() failed";
        mpg123_close(mh);
        mpg123_delete(mh);
        return AudioBuffer{};
    }

    // Ensure we get float output
    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, MPG123_ENC_FLOAT_32);

    // Decode in a loop
    std::vector<float> pcmData;
    constexpr size_t kBufSize = 16384;
    std::vector<unsigned char> readBuf(kBufSize);
    size_t done = 0;

    int ret;
    do {
        ret = mpg123_read(mh, readBuf.data(), kBufSize, &done);
        if (done > 0) {
            size_t floatCount = done / sizeof(float);
            const float *fPtr = reinterpret_cast<const float *>(readBuf.data());
            pcmData.insert(pcmData.end(), fPtr, fPtr + floatCount);
        }
    } while (ret == MPG123_OK);

    if (ret != MPG123_DONE && ret != MPG123_OK) {
        // MPG123_DONE is the normal end-of-stream indicator
        if (pcmData.empty()) {
            qWarning() << "Mp3Codec::decode: decoding failed —"
                       << mpg123_strerror(mh);
            mpg123_close(mh);
            mpg123_delete(mh);
            return AudioBuffer{};
        }
    }

    mpg123_close(mh);
    mpg123_delete(mh);

    int totalSamples = static_cast<int>(pcmData.size());
    int frames = totalSamples / channels;

    AudioBuffer buf;
    buf.data = new float[static_cast<size_t>(totalSamples)];
    std::memcpy(buf.data, pcmData.data(),
                static_cast<size_t>(totalSamples) * sizeof(float));
    buf.frames = frames;
    buf.channels = channels;
    buf.sampleRate = static_cast<int>(rate);
    return buf;

#else
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.decode(path);
#endif
}

// ============================================================
// MP3 encode
// ============================================================

bool Mp3Codec::encode(const AudioBuffer &buffer, const QString &path, int bitrate)
{
#ifdef HAVE_LAME
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "Mp3Codec::encode: invalid AudioBuffer";
        return false;
    }
    if (buffer.channels > 2) {
        qWarning() << "Mp3Codec::encode: LAME supports max 2 channels, got"
                   << buffer.channels;
        return false;
    }

    lame_t lame = lame_init();
    if (!lame) {
        qWarning() << "Mp3Codec::encode: lame_init() failed";
        return false;
    }

    lame_set_num_channels(lame, buffer.channels);
    lame_set_in_samplerate(lame, buffer.sampleRate);
    lame_set_brate(lame, bitrate);
    lame_set_quality(lame, 2);  // 0=best quality, 9=fastest
    lame_set_mode(lame, buffer.channels == 1 ? MONO : JOINT_STEREO);

    if (lame_init_params(lame) < 0) {
        qWarning() << "Mp3Codec::encode: lame_init_params() failed";
        lame_close(lame);
        return false;
    }

    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Mp3Codec::encode: cannot open" << path << "for writing";
        lame_close(lame);
        return false;
    }

    // MP3 buffer: worst case 1.25 * samples + 7200
    int totalFrames = buffer.frames;
    int mp3BufSize = static_cast<int>(1.25 * totalFrames) + 7200;
    std::vector<unsigned char> mp3Buf(static_cast<size_t>(mp3BufSize));

    int mp3Bytes;
    if (buffer.channels == 1) {
        // Mono: use the single-channel float encode
        mp3Bytes = lame_encode_buffer_ieee_float(
            lame,
            buffer.data,    // left channel
            nullptr,        // right channel (null for mono)
            totalFrames,
            mp3Buf.data(),
            mp3BufSize);
    } else {
        // Stereo: interleaved float data
        mp3Bytes = lame_encode_buffer_interleaved_ieee_float(
            lame,
            buffer.data,
            totalFrames,
            mp3Buf.data(),
            mp3BufSize);
    }

    if (mp3Bytes < 0) {
        qWarning() << "Mp3Codec::encode: lame_encode_buffer failed, code" << mp3Bytes;
        lame_close(lame);
        return false;
    }

    if (mp3Bytes > 0) {
        outFile.write(reinterpret_cast<const char *>(mp3Buf.data()), mp3Bytes);
    }

    // Flush remaining MP3 data
    int flushBytes = lame_encode_flush(lame, mp3Buf.data(), mp3BufSize);
    if (flushBytes > 0) {
        outFile.write(reinterpret_cast<const char *>(mp3Buf.data()), flushBytes);
    }

    lame_close(lame);
    return true;

#else
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.encode(buffer, path, QStringLiteral("libmp3lame"), bitrate);
#endif
}

} // namespace dawcast
