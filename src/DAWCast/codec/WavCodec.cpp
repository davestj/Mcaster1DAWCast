// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "WavCodec.h"

#include <QFile>
#include <QDebug>

#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>

namespace dawcast {

// ---------- WAV header structures ----------

#pragma pack(push, 1)
struct RiffHeader {
    char     riffId[4];    // "RIFF"
    uint32_t fileSize;     // file size - 8
    char     waveId[4];    // "WAVE"
};

struct FmtChunk {
    char     chunkId[4];   // "fmt "
    uint32_t chunkSize;    // 16 for PCM, 18 for float
    uint16_t audioFormat;  // 1=PCM int, 3=IEEE float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;     // sampleRate * numChannels * bitsPerSample/8
    uint16_t blockAlign;   // numChannels * bitsPerSample/8
    uint16_t bitsPerSample;
};

struct DataChunkHeader {
    char     chunkId[4];   // "data"
    uint32_t chunkSize;
};
#pragma pack(pop)

// ---------- Helpers for sample format conversion ----------

static inline float int8ToFloat(int8_t s)
{
    // WAV 8-bit is unsigned (0..255), center at 128
    return (static_cast<float>(static_cast<uint8_t>(s)) - 128.0f) / 128.0f;
}

static inline float uint8ToFloat(uint8_t s)
{
    return (static_cast<float>(s) - 128.0f) / 128.0f;
}

static inline float int16ToFloat(int16_t s)
{
    return static_cast<float>(s) / 32768.0f;
}

static inline float int24ToFloat(const uint8_t *p)
{
    // Little-endian 24-bit signed
    int32_t val = static_cast<int32_t>(p[0])
                | (static_cast<int32_t>(p[1]) << 8)
                | (static_cast<int32_t>(p[2]) << 16);
    // Sign-extend from 24 to 32 bits
    if (val & 0x800000)
        val |= static_cast<int32_t>(0xFF000000u);
    return static_cast<float>(val) / 8388608.0f;
}

static inline float int32ToFloat(int32_t s)
{
    return static_cast<float>(s) / 2147483648.0f;
}

// ---------- Decode ----------

AudioBuffer WavCodec::decode(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "WavCodec::decode: cannot open" << path;
        return AudioBuffer{};
    }

    // Read RIFF header
    RiffHeader riff;
    if (file.read(reinterpret_cast<char *>(&riff), sizeof(riff)) != sizeof(riff)) {
        qWarning() << "WavCodec::decode: truncated RIFF header";
        return AudioBuffer{};
    }
    if (std::memcmp(riff.riffId, "RIFF", 4) != 0 ||
        std::memcmp(riff.waveId, "WAVE", 4) != 0) {
        qWarning() << "WavCodec::decode: not a valid WAV file";
        return AudioBuffer{};
    }

    // Scan chunks for "fmt " and "data"
    FmtChunk fmt{};
    bool fmtFound = false;
    DataChunkHeader dataHdr{};
    bool dataFound = false;
    qint64 dataOffset = 0;

    while (!file.atEnd()) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (file.read(chunkId, 4) != 4) break;
        if (file.read(reinterpret_cast<char *>(&chunkSize), 4) != 4) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            qint64 toRead = std::min(static_cast<qint64>(chunkSize),
                                     static_cast<qint64>(sizeof(FmtChunk) - 8));
            std::memcpy(fmt.chunkId, chunkId, 4);
            fmt.chunkSize = chunkSize;
            if (file.read(reinterpret_cast<char *>(&fmt.audioFormat), toRead) != toRead) {
                qWarning() << "WavCodec::decode: truncated fmt chunk";
                return AudioBuffer{};
            }
            // Skip any extra fmt bytes (e.g. extension for float format)
            qint64 remaining = static_cast<qint64>(chunkSize) - toRead;
            if (remaining > 0) file.skip(remaining);
            fmtFound = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            std::memcpy(dataHdr.chunkId, chunkId, 4);
            dataHdr.chunkSize = chunkSize;
            dataOffset = file.pos();
            dataFound = true;
            break; // data chunk found, stop scanning
        } else {
            // Skip unknown chunk (pad to even boundary)
            uint32_t skip = chunkSize + (chunkSize & 1);
            file.skip(skip);
        }
    }

    if (!fmtFound || !dataFound) {
        qWarning() << "WavCodec::decode: missing fmt or data chunk";
        return AudioBuffer{};
    }

    // Validate format
    uint16_t format = fmt.audioFormat;
    uint16_t bps = fmt.bitsPerSample;
    uint16_t ch = fmt.numChannels;
    uint32_t sr = fmt.sampleRate;

    if (format != 1 && format != 3) {
        qWarning() << "WavCodec::decode: unsupported format tag" << format;
        return AudioBuffer{};
    }
    if (format == 1 && bps != 8 && bps != 16 && bps != 24 && bps != 32) {
        qWarning() << "WavCodec::decode: unsupported PCM bit depth" << bps;
        return AudioBuffer{};
    }
    if (format == 3 && bps != 32) {
        qWarning() << "WavCodec::decode: unsupported float bit depth" << bps;
        return AudioBuffer{};
    }
    if (ch == 0 || sr == 0) {
        qWarning() << "WavCodec::decode: invalid channel count or sample rate";
        return AudioBuffer{};
    }

    int bytesPerSample = bps / 8;
    int blockAlign = bytesPerSample * ch;
    int totalFrames = static_cast<int>(dataHdr.chunkSize / static_cast<uint32_t>(blockAlign));

    if (totalFrames <= 0) {
        qWarning() << "WavCodec::decode: no audio frames in data chunk";
        return AudioBuffer{};
    }

    // Allocate output
    int totalSamples = totalFrames * ch;
    std::unique_ptr<float[]> outData(new float[static_cast<size_t>(totalSamples)]);

    // Read raw audio data
    file.seek(dataOffset);
    qint64 rawBytes = static_cast<qint64>(dataHdr.chunkSize);
    QByteArray rawData = file.read(rawBytes);
    if (rawData.size() < rawBytes) {
        // Truncated, adjust frame count
        totalFrames = static_cast<int>(rawData.size() / blockAlign);
        totalSamples = totalFrames * ch;
    }

    const uint8_t *src = reinterpret_cast<const uint8_t *>(rawData.constData());

    // Convert to float32
    if (format == 3) {
        // IEEE float 32-bit: direct copy
        std::memcpy(outData.get(), src, static_cast<size_t>(totalSamples) * sizeof(float));
    } else if (bps == 8) {
        for (int i = 0; i < totalSamples; ++i)
            outData[i] = uint8ToFloat(src[i]);
    } else if (bps == 16) {
        const int16_t *src16 = reinterpret_cast<const int16_t *>(src);
        for (int i = 0; i < totalSamples; ++i)
            outData[i] = int16ToFloat(src16[i]);
    } else if (bps == 24) {
        for (int i = 0; i < totalSamples; ++i)
            outData[i] = int24ToFloat(src + i * 3);
    } else if (bps == 32) {
        const int32_t *src32 = reinterpret_cast<const int32_t *>(src);
        for (int i = 0; i < totalSamples; ++i)
            outData[i] = int32ToFloat(src32[i]);
    }

    AudioBuffer buf;
    buf.data = outData.release();
    buf.frames = totalFrames;
    buf.channels = ch;
    buf.sampleRate = static_cast<int>(sr);
    return buf;
}

// ---------- Encode ----------

bool WavCodec::encode(const AudioBuffer &buffer, const QString &path, int bitDepth)
{
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "WavCodec::encode: invalid AudioBuffer";
        return false;
    }

    // Validate bit depth
    uint16_t bps;
    uint16_t audioFormat;
    if (bitDepth == 32) {
        // 32-bit float
        bps = 32;
        audioFormat = 3; // IEEE float
    } else if (bitDepth == 8 || bitDepth == 16 || bitDepth == 24) {
        bps = static_cast<uint16_t>(bitDepth);
        audioFormat = 1; // PCM integer
    } else {
        qWarning() << "WavCodec::encode: unsupported bit depth" << bitDepth
                    << "— using 16";
        bps = 16;
        audioFormat = 1;
    }

    uint16_t ch = static_cast<uint16_t>(buffer.channels);
    uint32_t sr = static_cast<uint32_t>(buffer.sampleRate);
    int bytesPerSample = bps / 8;
    uint16_t blockAlign = static_cast<uint16_t>(ch * bytesPerSample);
    uint32_t byteRate = sr * blockAlign;
    int totalSamples = buffer.frames * buffer.channels;
    uint32_t dataSize = static_cast<uint32_t>(totalSamples) * static_cast<uint32_t>(bytesPerSample);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "WavCodec::encode: cannot open" << path << "for writing";
        return false;
    }

    // Build RIFF header
    RiffHeader riff;
    std::memcpy(riff.riffId, "RIFF", 4);
    riff.fileSize = 4 + (8 + 16) + (8 + dataSize); // "WAVE" + fmt chunk + data chunk
    std::memcpy(riff.waveId, "WAVE", 4);

    // Build fmt chunk
    FmtChunk fmt;
    std::memcpy(fmt.chunkId, "fmt ", 4);
    fmt.chunkSize = 16;
    fmt.audioFormat = audioFormat;
    fmt.numChannels = ch;
    fmt.sampleRate = sr;
    fmt.byteRate = byteRate;
    fmt.blockAlign = blockAlign;
    fmt.bitsPerSample = bps;

    // Build data chunk header
    DataChunkHeader dataHdr;
    std::memcpy(dataHdr.chunkId, "data", 4);
    dataHdr.chunkSize = dataSize;

    // Write headers
    file.write(reinterpret_cast<const char *>(&riff), sizeof(riff));
    file.write(reinterpret_cast<const char *>(&fmt), 8 + 16); // chunkId + chunkSize + 16 bytes payload
    file.write(reinterpret_cast<const char *>(&dataHdr), sizeof(dataHdr));

    // Convert float32 to target format and write
    if (audioFormat == 3) {
        // IEEE float: write directly
        file.write(reinterpret_cast<const char *>(buffer.data),
                   static_cast<qint64>(totalSamples) * sizeof(float));
    } else {
        // Integer PCM — write in chunks to avoid massive allocation
        constexpr int kChunkSamples = 8192;
        QByteArray chunk;
        chunk.resize(kChunkSamples * bytesPerSample);

        int written = 0;
        while (written < totalSamples) {
            int count = std::min(kChunkSamples, totalSamples - written);
            char *dst = chunk.data();

            for (int i = 0; i < count; ++i) {
                float s = buffer.data[written + i];
                // Clamp to [-1, 1]
                s = std::fmax(-1.0f, std::fmin(1.0f, s));

                if (bps == 8) {
                    // 8-bit unsigned
                    uint8_t val = static_cast<uint8_t>(
                        std::lroundf(s * 127.0f) + 128);
                    dst[i] = static_cast<char>(val);
                } else if (bps == 16) {
                    int16_t val = static_cast<int16_t>(
                        std::lroundf(s * 32767.0f));
                    std::memcpy(dst + i * 2, &val, 2);
                } else if (bps == 24) {
                    int32_t val = static_cast<int32_t>(
                        std::lround(static_cast<double>(s) * 8388607.0));
                    uint8_t *p = reinterpret_cast<uint8_t *>(dst + i * 3);
                    p[0] = static_cast<uint8_t>(val & 0xFF);
                    p[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
                    p[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
                }
            }

            file.write(dst, static_cast<qint64>(count) * bytesPerSample);
            written += count;
        }
    }

    return true;
}

} // namespace dawcast
