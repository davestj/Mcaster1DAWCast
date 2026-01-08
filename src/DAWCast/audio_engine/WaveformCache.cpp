// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "WaveformCache.h"
#include "../codec/FFmpegCodec.h"
#include "../core/AudioBuffer.h"

#include <QDebug>
#include <QMetaObject>
#include <QRunnable>
#include <QMutexLocker>

#include <cmath>
#include <algorithm>

namespace dawcast {

// ── Singleton ──────────────────────────────────────────────────────────────

WaveformCache* WaveformCache::instance()
{
    static WaveformCache s_instance;
    return &s_instance;
}

WaveformCache::WaveformCache(QObject* parent)
    : QObject(parent)
{
    m_threadPool.setMaxThreadCount(2);
}

WaveformCache::~WaveformCache() = default;

// ── Public API ─────────────────────────────────────────────────────────────

const WaveformData* WaveformCache::getWaveform(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_cache.constFind(filePath);
    if (it != m_cache.constEnd()) {
        // Update LRU order
        m_accessOrder.removeAll(filePath);
        m_accessOrder.append(filePath);
        return &it.value();
    }
    lock.unlock();

    // Not cached — request async decode
    requestWaveform(filePath);
    return nullptr;
}

bool WaveformCache::hasWaveform(const QString& filePath) const
{
    QMutexLocker lock(&m_mutex);
    return m_cache.contains(filePath);
}

void WaveformCache::requestWaveform(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);

    // Already cached or already being decoded
    if (m_cache.contains(filePath) || m_pending.contains(filePath))
        return;

    m_pending.insert(filePath, true);
    lock.unlock();

    // Run decode on thread pool
    class DecodeTask : public QRunnable {
    public:
        DecodeTask(WaveformCache* cache, const QString& path)
            : m_cache(cache), m_path(path) {}

        void run() override
        {
            m_cache->decodeAndCache(m_path);
        }

    private:
        WaveformCache* m_cache;
        QString        m_path;
    };

    auto* task = new DecodeTask(this, filePath);
    task->setAutoDelete(true);
    m_threadPool.start(task);
}

void WaveformCache::clearCache()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
    m_accessOrder.clear();
}

// ── Private ────────────────────────────────────────────────────────────────

void WaveformCache::decodeAndCache(const QString& filePath)
{
    // Decode the audio file to float32 PCM
    FFmpegCodec codec;
    AudioBuffer buf = codec.decode(filePath);

    if (!buf.data || buf.frames <= 0 || buf.channels <= 0) {
        qWarning() << "WaveformCache: decode failed for" << filePath;
        QMutexLocker lock(&m_mutex);
        m_pending.remove(filePath);
        return;
    }

    // Build peak/RMS data
    WaveformData waveform;
    waveform.blockSize   = kBlockSize;
    waveform.channels    = buf.channels;
    waveform.sampleRate  = buf.sampleRate;
    waveform.totalFrames = buf.frames;

    int64_t totalFrames = buf.frames;
    int channels = buf.channels;
    int numBlocks = static_cast<int>((totalFrames + kBlockSize - 1) / kBlockSize);

    waveform.peaks.resize(static_cast<size_t>(numBlocks), 0.0f);
    waveform.rms.resize(static_cast<size_t>(numBlocks), 0.0f);

    for (int b = 0; b < numBlocks; ++b) {
        int64_t startFrame = static_cast<int64_t>(b) * kBlockSize;
        int64_t endFrame   = std::min(startFrame + kBlockSize, totalFrames);
        int framesInBlock  = static_cast<int>(endFrame - startFrame);

        float maxAbs  = 0.0f;
        float sumSq   = 0.0f;
        int   samples = framesInBlock * channels;

        for (int64_t f = startFrame; f < endFrame; ++f) {
            // Take max across all channels for mono waveform display
            float frameMax = 0.0f;
            float frameSumSq = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                float s = buf.data[f * channels + ch];
                float a = std::fabs(s);
                if (a > frameMax) frameMax = a;
                frameSumSq += s * s;
            }
            if (frameMax > maxAbs) maxAbs = frameMax;
            sumSq += frameSumSq;
        }

        waveform.peaks[static_cast<size_t>(b)] = maxAbs;
        waveform.rms[static_cast<size_t>(b)]   =
            std::sqrt(sumSq / static_cast<float>(samples));
    }

    // Free the decoded audio buffer
    delete[] buf.data;
    buf.data = nullptr;

    // Store in cache (with LRU eviction if needed)
    {
        QMutexLocker lock(&m_mutex);
        m_pending.remove(filePath);
        m_cache.insert(filePath, std::move(waveform));
        m_accessOrder.removeAll(filePath);
        m_accessOrder.append(filePath);
        evictLRU();
    }

    // Emit signal on the GUI thread
    QMetaObject::invokeMethod(this, [this, filePath]() {
        emit waveformReady(filePath);
    }, Qt::QueuedConnection);
}

void WaveformCache::evictLRU()
{
    // Called with m_mutex already held
    while (m_cache.size() > kMaxCacheEntries && !m_accessOrder.isEmpty()) {
        QString oldest = m_accessOrder.takeFirst();
        m_cache.remove(oldest);
    }
}

} // namespace dawcast
