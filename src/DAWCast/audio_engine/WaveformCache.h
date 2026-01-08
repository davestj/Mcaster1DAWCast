// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QThreadPool>
#include <QString>

#include <cstdint>
#include <vector>

namespace dawcast {

struct WaveformData {
    std::vector<float> peaks;    // max absolute sample per block
    std::vector<float> rms;      // RMS per block
    int blockSize    = 256;      // samples per block
    int channels     = 0;
    int sampleRate   = 0;
    int64_t totalFrames = 0;
};

class WaveformCache : public QObject {
    Q_OBJECT

public:
    static WaveformCache* instance();

    /// Returns cached waveform data, or nullptr if not yet decoded.
    /// If not cached, automatically starts a background decode.
    const WaveformData* getWaveform(const QString& filePath);

    /// Check if waveform data is already cached for a file.
    bool hasWaveform(const QString& filePath) const;

    /// Request async waveform decode. Emits waveformReady when done.
    void requestWaveform(const QString& filePath);

    /// Clear all cached entries.
    void clearCache();

signals:
    void waveformReady(const QString& filePath);

private:
    explicit WaveformCache(QObject* parent = nullptr);
    ~WaveformCache() override;

    void decodeAndCache(const QString& filePath);
    void evictLRU();

    mutable QMutex m_mutex;
    QHash<QString, WaveformData> m_cache;
    QHash<QString, bool>         m_pending;  // tracks in-progress decodes
    QStringList                  m_accessOrder; // LRU tracking (most recent at back)
    QThreadPool                  m_threadPool;

    static constexpr int kMaxCacheEntries = 100;
    static constexpr int kBlockSize       = 256;
};

} // namespace dawcast
