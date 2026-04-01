// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QImage>
#include <QList>
#include <QTimer>
#include <vector>

#include "ForensicDetector.h"

namespace dawcast::editor {

/// Display mode for the spectral analysis panel.
enum class SpectralDisplayMode {
    Spectrogram,   ///< Time-frequency heat map (X=time, Y=freq, color=magnitude)
    Spectrum,      ///< Single-frame frequency spectrum (bar/line)
    Waterfall      ///< 3D-style scrolling waterfall
};

/// FFT spectrogram and frequency analysis panel for forensic audio inspection.
/// Renders a color-coded time-frequency display using the existing FFT class.
class SpectralAnalysisPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SpectralAnalysisPanel(QWidget* parent = nullptr);
    ~SpectralAnalysisPanel() override;

    /// Set the decoded audio data for spectral analysis.
    void setAudioData(const float* data, int64_t frames, int channels, int sampleRate);

    /// Update the playback cursor position (in samples).
    void setPosition(int64_t samplePosition);

    /// Set the FFT window size (256 to 65536, power of 2).
    void setFFTSize(int size);

    /// Set the spectral display mode.
    void setDisplayMode(SpectralDisplayMode mode);

    // ── Forensic detection (delegates to ForensicDetector) ───────────────

    QList<DetectionMarker> runFrequencyDetection(double minFreq, double maxFreq,
                                                  double thresholdDb);
    QList<DetectionMarker> runAnomalyDetection();
    QList<DetectionMarker> runEVPDetection();
    QList<DetectionMarker> runInfrasonicScan();
    QList<DetectionMarker> runUltrasonicScan();

    /// Get current detection markers.
    const QList<DetectionMarker>& markers() const { return m_markers; }

signals:
    /// Emitted when a marker is clicked in the spectrogram.
    void markerClicked(const DetectionMarker& marker);
    /// Emitted when analysis is complete.
    void analysisComplete(const QList<DetectionMarker>& markers);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /// Rebuild the spectrogram image from audio data.
    void rebuildSpectrogram();

    /// Map a magnitude (dB) to a heat-map color.
    QRgb magnitudeToColor(float dB) const;

    /// Convert sample position to X pixel coordinate in the spectrogram.
    int sampleToX(int64_t sample) const;

    const float* m_data       = nullptr;
    int64_t      m_frames     = 0;
    int          m_channels   = 0;
    int          m_sampleRate = 44100;

    int m_fftSize = 4096;
    SpectralDisplayMode m_displayMode = SpectralDisplayMode::Spectrogram;

    int64_t m_cursorPosition = 0;

    QImage m_spectrogramImage;   ///< Pre-rendered spectrogram bitmap
    bool   m_spectrogramDirty = true;

    QList<DetectionMarker> m_markers;

    // Waterfall history for waterfall mode
    std::vector<std::vector<float>> m_waterfallHistory;
};

} // namespace dawcast::editor
