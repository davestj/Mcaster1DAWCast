// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QElapsedTimer>
#include <QThread>
#include <QMutex>
#include <atomic>
#include <cstdint>
#include <memory>

namespace dawcast {

template <typename T> class RingBuffer;

/// RTMP streaming output using FFmpeg's libavformat.
///
/// Supports audio-only streaming (AAC over RTMP/FLV) and combined
/// audio+video streaming (H.264 + AAC). Compatible with YouTube Live,
/// Twitch, Facebook Live, and any standard RTMP ingest server.
///
/// Thread safety:
///   - pushAudioFrame() is called from the PortAudio audio thread (RT-safe path).
///   - pushVideoFrame() is called from the video render thread.
///   - startStreaming() / stopStreaming() are called from the GUI thread.
///   - An internal processing thread reads from ring buffers, encodes, and
///     writes to the RTMP stream.
class RTMPStreamer : public QObject
{
    Q_OBJECT

public:
    explicit RTMPStreamer(QObject* parent = nullptr);
    ~RTMPStreamer() override;

    /// Configuration for an RTMP streaming session.
    struct StreamConfig {
        QString url;           ///< e.g. rtmp://live.twitch.tv/app/STREAM_KEY
        QString streamKey;     ///< Appended to URL if separate from URL

        // Audio
        QString audioCodec    = QStringLiteral("aac");
        int     audioBitrate  = 128;    ///< kbps
        int     audioSampleRate = 44100;
        int     audioChannels = 2;

        // Video (optional -- audio-only streaming also supported)
        bool    enableVideo   = false;
        QString videoCodec    = QStringLiteral("h264");
        int     videoBitrate  = 2500;   ///< kbps
        int     videoWidth    = 1920;
        int     videoHeight   = 1080;
        double  videoFps      = 30.0;

        /// Platform presets -- selecting a platform auto-fills URL template
        /// and recommended encoding settings.
        enum Platform { Custom, YouTube, Twitch, Facebook, Icecast };
        Platform platform = Custom;
    };

    void setConfig(const StreamConfig& config);
    [[nodiscard]] StreamConfig config() const;

    bool startStreaming();
    void stopStreaming();
    [[nodiscard]] bool isStreaming() const;

    /// Feed interleaved float audio samples from the audio thread.
    /// This is RT-safe: writes into a lock-free SPSC ring buffer.
    void pushAudioFrame(const float* samples, int frames, int channels, int sampleRate);

    /// Feed a video frame from the video render thread.
    /// The image is converted to the encoder's pixel format internally.
    void pushVideoFrame(const QImage& frame);

    // -- Statistics --
    [[nodiscard]] int64_t bytesSent() const;
    [[nodiscard]] double  uptimeSeconds() const;
    [[nodiscard]] int     droppedFrames() const;
    [[nodiscard]] double  currentBitrateKbps() const;

signals:
    void connected();
    void disconnected();
    void error(const QString& message);
    void statsUpdated(int64_t bytesSent, double bitrateKbps, int droppedFrames);

private:
    class ProcessingThread;
    friend class ProcessingThread;

    /// Build the full RTMP URL from url + streamKey.
    [[nodiscard]] QString buildUrl() const;

    StreamConfig m_config;

    // Ring buffer for audio samples (float, interleaved).
    // Sized for ~2 seconds of audio at 48 kHz stereo.
    std::unique_ptr<RingBuffer<float>> m_audioRingBuffer;

    // Video frame queue (mutex-protected, bounded).
    QMutex           m_videoMutex;
    QList<QImage>    m_videoQueue;
    static constexpr int kMaxVideoQueueSize = 4;

    // Processing thread handles encoding + RTMP output.
    ProcessingThread* m_thread = nullptr;

    // Statistics (atomics for cross-thread access).
    std::atomic<int64_t> m_bytesSent{0};
    std::atomic<int>     m_droppedFrames{0};
    std::atomic<bool>    m_streaming{false};
    QElapsedTimer        m_uptimeTimer;

    // For bitrate calculation
    std::atomic<int64_t> m_bytesAtLastSample{0};
    std::atomic<double>  m_currentBitrate{0.0};
};

} // namespace dawcast
