// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AacCodec.h"
#include "FFmpegCodec.h"

#include <QDebug>
#include <QFile>

#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef HAVE_FDKAAC
#include <fdk-aac/aacenc_lib.h>
#endif

namespace dawcast {

// ============================================================
// AAC decode — always uses FFmpeg (fdk-aac has no file decoder API)
// ============================================================

AudioBuffer AacCodec::decode(const QString &path)
{
    FFmpegCodec ffmpeg;
    return ffmpeg.decode(path);
}

// ============================================================
// AAC encode
// ============================================================

bool AacCodec::encode(const AudioBuffer &buffer, const QString &path, int bitrate)
{
#ifdef HAVE_FDKAAC
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "AacCodec::encode: invalid AudioBuffer";
        return false;
    }
    if (buffer.channels > 2) {
        qWarning() << "AacCodec::encode: only mono/stereo supported, got"
                   << buffer.channels;
        return false;
    }

    HANDLE_AACENCODER encoder = nullptr;
    AACENC_ERROR err;

    err = aacEncOpen(&encoder, 0, static_cast<UINT>(buffer.channels));
    if (err != AACENC_OK) {
        qWarning() << "AacCodec::encode: aacEncOpen() failed, error" << err;
        return false;
    }

    // Set encoder parameters
    aacEncoder_SetParam(encoder, AACENC_AOT, 2);  // AAC-LC
    aacEncoder_SetParam(encoder, AACENC_BITRATE, static_cast<UINT>(bitrate * 1000));
    aacEncoder_SetParam(encoder, AACENC_SAMPLERATE, static_cast<UINT>(buffer.sampleRate));
    aacEncoder_SetParam(encoder, AACENC_CHANNELMODE,
                        buffer.channels == 1 ? MODE_1 : MODE_2);
    aacEncoder_SetParam(encoder, AACENC_TRANSMUX, 2);  // ADTS output

    err = aacEncEncode(encoder, nullptr, nullptr, nullptr, nullptr);
    if (err != AACENC_OK) {
        qWarning() << "AacCodec::encode: encoder init failed, error" << err;
        aacEncClose(&encoder);
        return false;
    }

    // Get encoder info for frame sizes
    AACENC_InfoStruct encInfo;
    std::memset(&encInfo, 0, sizeof(encInfo));
    err = aacEncInfo(encoder, &encInfo);
    if (err != AACENC_OK) {
        qWarning() << "AacCodec::encode: aacEncInfo() failed";
        aacEncClose(&encoder);
        return false;
    }

    int frameSize = static_cast<int>(encInfo.frameLength);  // samples per channel per frame
    int inputBufSize = frameSize * buffer.channels;

    // Convert float32 [-1,1] to int16 for fdk-aac
    int totalSamples = buffer.frames * buffer.channels;
    std::vector<int16_t> pcm16(static_cast<size_t>(totalSamples));
    for (int i = 0; i < totalSamples; ++i) {
        float s = std::fmax(-1.0f, std::fmin(1.0f, buffer.data[i]));
        pcm16[static_cast<size_t>(i)] = static_cast<int16_t>(std::lroundf(s * 32767.0f));
    }

    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "AacCodec::encode: cannot open" << path;
        aacEncClose(&encoder);
        return false;
    }

    // Output buffer for encoded ADTS frames
    int outBufSize = static_cast<int>(encInfo.maxOutBufBytes);
    std::vector<uint8_t> outBuf(static_cast<size_t>(outBufSize));

    int samplesRead = 0;
    bool success = true;

    while (samplesRead < totalSamples || samplesRead == 0) {
        // Prepare input
        int remaining = totalSamples - samplesRead;
        int thisChunk = std::min(inputBufSize, remaining);

        // Pad the last frame with silence if needed
        std::vector<int16_t> inputFrame(static_cast<size_t>(inputBufSize), 0);
        if (thisChunk > 0) {
            std::memcpy(inputFrame.data(), pcm16.data() + samplesRead,
                        static_cast<size_t>(thisChunk) * sizeof(int16_t));
        }

        // Set up input buffer descriptor
        AACENC_BufDesc inBufDesc;
        std::memset(&inBufDesc, 0, sizeof(inBufDesc));
        int inBufId = IN_AUDIO_DATA;
        int inBufElSize = static_cast<int>(sizeof(int16_t));
        int inBufSizeBytes = inputBufSize * inBufElSize;
        void *inBufPtr = inputFrame.data();
        inBufDesc.numBufs = 1;
        inBufDesc.bufs = &inBufPtr;
        inBufDesc.bufferIdentifiers = &inBufId;
        inBufDesc.bufSizes = &inBufSizeBytes;
        inBufDesc.bufElSizes = &inBufElSize;

        // Set up output buffer descriptor
        AACENC_BufDesc outBufDesc;
        std::memset(&outBufDesc, 0, sizeof(outBufDesc));
        int outBufId = OUT_BITSTREAM_DATA;
        int outBufElSize = 1;
        int outBufSizeInt = outBufSize;
        void *outBufPtr = outBuf.data();
        outBufDesc.numBufs = 1;
        outBufDesc.bufs = &outBufPtr;
        outBufDesc.bufferIdentifiers = &outBufId;
        outBufDesc.bufSizes = &outBufSizeInt;
        outBufDesc.bufElSizes = &outBufElSize;

        AACENC_InArgs inArgs;
        std::memset(&inArgs, 0, sizeof(inArgs));
        inArgs.numInSamples = (thisChunk > 0) ? inputBufSize : -1; // -1 signals flush

        AACENC_OutArgs outArgs;
        std::memset(&outArgs, 0, sizeof(outArgs));

        err = aacEncEncode(encoder, &inBufDesc, &outBufDesc, &inArgs, &outArgs);
        if (err != AACENC_OK && err != AACENC_ENCODE_EOF) {
            qWarning() << "AacCodec::encode: aacEncEncode() failed, error" << err;
            success = false;
            break;
        }

        if (outArgs.numOutBytes > 0) {
            outFile.write(reinterpret_cast<const char *>(outBuf.data()),
                          outArgs.numOutBytes);
        }

        if (thisChunk > 0) {
            samplesRead += inputBufSize;
        } else {
            break; // flush complete
        }

        if (err == AACENC_ENCODE_EOF) break;
    }

    // Flush encoder
    {
        AACENC_BufDesc outBufDesc;
        std::memset(&outBufDesc, 0, sizeof(outBufDesc));
        int outBufId = OUT_BITSTREAM_DATA;
        int outBufElSize = 1;
        int outBufSizeInt = outBufSize;
        void *outBufPtr = outBuf.data();
        outBufDesc.numBufs = 1;
        outBufDesc.bufs = &outBufPtr;
        outBufDesc.bufferIdentifiers = &outBufId;
        outBufDesc.bufSizes = &outBufSizeInt;
        outBufDesc.bufElSizes = &outBufElSize;

        AACENC_InArgs inArgs;
        std::memset(&inArgs, 0, sizeof(inArgs));
        inArgs.numInSamples = -1;

        AACENC_OutArgs outArgs;
        std::memset(&outArgs, 0, sizeof(outArgs));

        err = aacEncEncode(encoder, nullptr, &outBufDesc, &inArgs, &outArgs);
        if (outArgs.numOutBytes > 0) {
            outFile.write(reinterpret_cast<const char *>(outBuf.data()),
                          outArgs.numOutBytes);
        }
    }

    aacEncClose(&encoder);
    return success;

#else
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.encode(buffer, path, QStringLiteral("aac"), bitrate);
#endif
}

} // namespace dawcast
