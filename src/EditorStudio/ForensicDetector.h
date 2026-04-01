// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QImage>
#include <QColor>
#include <QList>
#include <QString>

namespace dawcast::editor {

/// A single detection marker placed at a specific sample position.
struct DetectionMarker {
    int64_t  positionSamples = 0;
    double   frequencyHz     = 0.0;
    double   magnitudeDb     = -120.0;
    QString  label;
    QColor   color           = Qt::red;
    int      frameIndex      = -1;   ///< For video orb detection (frame number)
};

/// Forensic analysis engine for audio and video anomaly detection.
/// Runs FFT-based frequency analysis, anomaly scoring, EVP formant detection,
/// infrasonic/ultrasonic scanning, and QPainter-based video orb detection.
class ForensicDetector : public QObject
{
    Q_OBJECT

public:
    explicit ForensicDetector(QObject* parent = nullptr);
    ~ForensicDetector() override;

    /// Set the audio data to analyze.
    void setAudioData(const float* data, int64_t frames, int channels, int sampleRate);

    // ── Audio detection methods ──────────────────────────────────────────

    /// Detect frequency peaks within [minHz..maxHz] above thresholdDb.
    QList<DetectionMarker> detectFrequencyRange(double minHz, double maxHz,
                                                 double thresholdDb);

    /// Scan for anomalous patterns: sudden spectral spikes, energy bursts
    /// in otherwise quiet regions. sensitivityPercent: 0..100.
    QList<DetectionMarker> detectAnomalies(double sensitivityPercent);

    /// Electronic Voice Phenomena: formant-like patterns (200Hz-4kHz)
    /// appearing in quiet/silent regions.
    QList<DetectionMarker> detectEVP(double sensitivity);

    /// Sub-audible vibrations in the 0.5Hz - 20Hz range.
    QList<DetectionMarker> detectInfrasonic();

    /// Above-20kHz content (requires sample rate > 40kHz).
    QList<DetectionMarker> detectUltrasonic();

    /// Phase discontinuities and anomalous stereo field changes.
    QList<DetectionMarker> detectPhaseAnomalies();

    // ── Video detection methods ──────────────────────────────────────────

    /// Detect bright circular anomalies (orbs) in video frames.
    /// Uses grayscale conversion, Gaussian blur, thresholding, and
    /// connected-component analysis via QPainter image processing.
    QList<DetectionMarker> detectOrbs(const QList<QImage>& frames,
                                      double sensitivity);

signals:
    void progress(int percent);
    void detectionComplete(const QList<DetectionMarker>& markers);

private:
    /// Compute magnitude spectrum (dB) for a window of audio starting at sampleOffset.
    void computeSpectrum(int64_t sampleOffset, int fftSize,
                         std::vector<float>& magnitudesDb);

    /// Compute RMS energy for a block of mono audio.
    double computeRMS(const float* mono, int64_t frames);

    /// Mix multi-channel audio to mono for analysis.
    void mixToMono(std::vector<float>& mono);

    const float* m_data       = nullptr;
    int64_t      m_frames     = 0;
    int          m_channels   = 0;
    int          m_sampleRate = 44100;

    std::vector<float> m_mono;  ///< Cached mono mix-down
    bool m_monoValid = false;
};

} // namespace dawcast::editor
