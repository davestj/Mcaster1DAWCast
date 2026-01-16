// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <vector>

#include "../codec/TagTransfer.h"

namespace dawcast {

class Timeline;
class AudioTrack;
class AudioClipReader;
class AudioMixer;
class DspChain;

/// Offline rendering engine that bounces the timeline to an audio
/// or audio+video file.  Runs the same clip-read / DSP / mix pipeline
/// as PlaybackEngine but without PortAudio — purely CPU-driven.
///
/// Usage:
///   ExportEngine exporter;
///   exporter.startExport(timeline, config);
///   // connect progress(int), finished(QString), error(QString)
class ExportEngine : public QObject
{
    Q_OBJECT

public:
    explicit ExportEngine(QObject* parent = nullptr);
    ~ExportEngine() override;

    struct ExportConfig {
        QString outputPath;
        QString audioCodec;         // "mp3", "aac", "wav", "flac", "opus", "vorbis"
        int     audioBitrate = 256; // kbps (ignored for lossless)
        int     sampleRate   = 48000;
        int     channels     = 2;
        QString videoCodec;         // "h264", "vp9", "" for audio-only
        int     videoBitrate = 5000; // kbps
        int     videoWidth   = 1920;
        int     videoHeight  = 1080;
        double  videoFps     = 30.0;
        QString container;          // "mp4", "mkv", "webm", "wav", "mp3"

        // Metadata to embed in the exported file (optional).
        // If non-empty, these tags will be written after export completes.
        AudioTags metadata;
    };

    /// Begin exporting the timeline with the given configuration.
    /// Emits progress(), finished(), or error() as the export proceeds.
    void startExport(Timeline* timeline, const ExportConfig& config);

    /// Request cancellation. The engine will stop at the next block
    /// boundary and emit error("Export cancelled").
    void cancelExport();

    [[nodiscard]] bool isExporting() const;

signals:
    /// 0..100 progress indicator.
    void progress(int percent);

    /// Export completed successfully.
    void finished(const QString& outputPath);

    /// Export failed or was cancelled.
    void error(const QString& message);

private:
    /// Audio-only bounce: read clips, apply DSP, mix, encode.
    void exportAudioOnly(Timeline* timeline, const ExportConfig& config);

    /// Audio + video bounce: same audio pipeline, plus video compositing.
    void exportAudioVideo(Timeline* timeline, const ExportConfig& config);

    /// Internal per-track state used during offline render.
    struct TrackState {
        AudioTrack*                     audioTrack = nullptr;
        std::vector<AudioClipReader*>   readers;
        std::vector<float>              buffer;     // per-track scratch (interleaved)
    };

    /// Build offline readers for all audio tracks in the timeline.
    std::vector<TrackState> buildTrackStates(Timeline* timeline);

    /// Release readers allocated by buildTrackStates.
    void freeTrackStates(std::vector<TrackState>& states);

    /// Render one block of mixed audio into outBuf.
    /// Returns the number of samples written (frames * channels).
    int renderBlock(std::vector<TrackState>& tracks,
                    int64_t position, int frames, int channels,
                    int sampleRate, std::vector<float>& outBuf);

    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_exporting{false};
};

} // namespace dawcast
