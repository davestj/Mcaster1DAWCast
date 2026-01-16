// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <atomic>

namespace dawcast {

// ---------------------------------------------------------------------------
// BatchJob — one file to encode
// ---------------------------------------------------------------------------

struct BatchJob
{
    QString inputPath;
    QString outputPath;
    QString outputCodec;        // "mp3", "aac", "opus", "flac", "wav", "vorbis"
    int     bitrate     = 192;  // kbps (ignored for lossless)
    int     sampleRate  = 0;    // 0 = keep source sample rate
    int     channels    = 0;    // 0 = keep source channel count

    bool    applyDspChain = false;
    QStringList dspPresets;     // YAML preset paths to load and apply in order

    float   targetLUFS  = 0.0f; // 0 = no normalization
                                // -14 = streaming, -16 = podcast, -23 = EBU R128

    bool    copyTags    = true; // Preserve metadata tags from source file

    enum Status { Pending, Processing, Complete, Failed, Cancelled };
    Status  status          = Pending;
    int     progressPercent = 0;
    QString errorMessage;

    bool operator==(const BatchJob& o) const { return inputPath == o.inputPath && outputPath == o.outputPath; }

    // Source metadata (filled after analysis)
    int     srcSampleRate = 0;
    int     srcChannels   = 0;
    int     srcFrames     = 0;
    QString srcFormat;          // e.g. "MP3", "WAV", "FLAC"
    double  durationSec   = 0.0;
};

// ---------------------------------------------------------------------------
// BatchEncoder — queues and processes multiple encode jobs
// ---------------------------------------------------------------------------

class BatchEncoder : public QObject
{
    Q_OBJECT

public:
    explicit BatchEncoder(QObject* parent = nullptr);
    ~BatchEncoder() override;

    // ── Job management ────────────────────────────────────────────────────
    void addJob(const BatchJob& job);
    void removeJob(int index);
    void clearJobs();
    int  jobCount() const;
    const BatchJob& job(int index) const;
    BatchJob& jobRef(int index);

    // ── Encoding control ──────────────────────────────────────────────────
    void startEncoding();       // Process all pending jobs sequentially
    void cancelAll();
    [[nodiscard]] bool isEncoding() const;

    // ── Settings ──────────────────────────────────────────────────────────
    void setParallelJobs(int count);
    int  parallelJobs() const;
    void setOutputDirectory(const QString& dir);
    QString outputDirectory() const;

signals:
    void jobStarted(int index);
    void jobProgress(int index, int percent);
    void jobFinished(int index);
    void jobFailed(int index, const QString& error);
    void allFinished();
    void batchProgress(int completedJobs, int totalJobs);

private:
    void processJob(int index);
    void encodeFile(BatchJob& job);

    /// Resolve FFmpeg codec name from the user-facing codec string.
    static QString resolveCodecName(const QString& codec);

    /// Resolve FFmpeg format (container) from the user-facing codec string.
    static QString resolveFormat(const QString& codec);

    QList<BatchJob>    m_jobs;
    std::atomic<bool>  m_cancelled{false};
    std::atomic<bool>  m_encoding{false};
    int                m_parallelJobs = 1;
    QString            m_outputDirectory;
};

} // namespace dawcast
