// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioWorker.h"
#include "../codec/FFmpegCodec.h"
#include "../codec/WavCodec.h"
#include "../codec/CodecRegistry.h"

#include <QFileInfo>
#include <QDebug>
#include <atomic>

namespace dawcast {

// Buffer size per decode chunk (in frames)
static constexpr int kDecodeChunkFrames = 4096;

AudioWorker::AudioWorker(QObject* parent)
    : QObject(parent)
{
}

AudioWorker::~AudioWorker()
{
    stop();
}

void AudioWorker::setSource(const QString& path)
{
    m_sourcePath = path;
}

void AudioWorker::start()
{
    if (m_running.load(std::memory_order_acquire)) return;
    if (m_sourcePath.isEmpty()) {
        emit error(QStringLiteral("No source path set"));
        return;
    }

    m_running.store(true, std::memory_order_release);

    const QFileInfo fi(m_sourcePath);
    if (!fi.exists() || !fi.isReadable()) {
        emit error(QStringLiteral("Cannot read source file: %1").arg(m_sourcePath));
        m_running.store(false, std::memory_order_release);
        return;
    }

    const QString ext = fi.suffix().toLower();
    qDebug() << "AudioWorker: Decoding" << m_sourcePath << "(format:" << ext << ")";

    // Decode the entire file into memory using the appropriate codec.
    // For streaming decode, we would use a pull-based FFmpeg demuxer loop,
    // but the codec API currently returns full AudioBuffers.
    AudioBuffer fullBuffer;
    bool decoded = false;

    if (ext == QStringLiteral("wav")) {
        WavCodec wavCodec;
        fullBuffer = wavCodec.decode(m_sourcePath);
        decoded = (fullBuffer.data != nullptr && fullBuffer.frames > 0);
    } else {
        // Use FFmpegCodec for all other formats (MP3, FLAC, OGG, AAC, etc.)
        FFmpegCodec ffCodec;
        fullBuffer = ffCodec.decode(m_sourcePath);
        decoded = (fullBuffer.data != nullptr && fullBuffer.frames > 0);
    }

    if (!decoded) {
        emit error(QStringLiteral("Failed to decode: %1").arg(m_sourcePath));
        m_running.store(false, std::memory_order_release);
        return;
    }

    // Emit decoded audio in chunks so the consumer can process incrementally
    // without blocking on the entire file.
    int offset = 0;
    while (offset < fullBuffer.frames && m_running.load(std::memory_order_acquire)) {
        int chunkFrames = qMin(kDecodeChunkFrames, fullBuffer.frames - offset);

        AudioBuffer chunk;
        chunk.frames     = chunkFrames;
        chunk.channels   = fullBuffer.channels;
        chunk.sampleRate = fullBuffer.sampleRate;

        // Allocate and copy chunk data
        int chunkSamples = chunkFrames * fullBuffer.channels;
        chunk.data = new float[chunkSamples];
        std::memcpy(chunk.data,
                    fullBuffer.data + (offset * fullBuffer.channels),
                    static_cast<size_t>(chunkSamples) * sizeof(float));

        emit bufferReady(chunk);
        offset += chunkFrames;
    }

    // Clean up the full decode buffer
    delete[] fullBuffer.data;
    fullBuffer.data = nullptr;

    m_running.store(false, std::memory_order_release);

    if (offset >= fullBuffer.frames) {
        emit finished();
    }
    // If stopped early (m_running was set to false), we just exit quietly.
}

void AudioWorker::stop()
{
    m_running.store(false, std::memory_order_release);
}

} // namespace dawcast
