// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QImage>
#include <QList>
#include <QTimer>

#include <atomic>

#include "ForensicDetector.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#else
typedef void PaStream;
#endif

namespace dawcast::editor {

/// Overlay analysis modes for the forensic waveform view.
enum class OverlayMode {
    None,
    SpectralView,       ///< Color-coded frequency spectrum overlay
    FrequencyDetect,    ///< Highlight specific frequency ranges
    PhaseAnalysis,      ///< Show phase relationships
    AnomalyScan,        ///< Auto-detect anomalous patterns
    EVPDetection,       ///< Electronic Voice Phenomena scan
    InfrasonicScan,     ///< Sub-20Hz detection
    UltrasonicScan,     ///< Above-20kHz detection (if sample rate supports)
    ParanormalMode,     ///< Combined: infrasonic + ultrasonic + anomaly + EVP
    OrbDetection        ///< Video frame analysis for luminous anomalies
};

/// Forensic-grade waveform view with extreme zoom (from full-file overview down
/// to individual sample dots), QPainter rendering, click+drag selection,
/// PortAudio playback, and multiple overlay analysis modes.
class ForensicWaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit ForensicWaveformView(QWidget* parent = nullptr);
    ~ForensicWaveformView() override;

    /// Load an audio file via FFmpegCodec and display its waveform.
    void loadFile(const QString& path);

    /// Direct access to decoded data (for sharing with SpectralAnalysisPanel).
    const float* audioData() const { return m_data; }
    int64_t audioFrames()    const { return m_frames; }
    int     audioChannels()  const { return m_channels; }
    int     audioSampleRate() const { return m_sampleRate; }

    /// True if a file is loaded.
    bool hasFile() const { return m_data != nullptr; }

    /// Get the loaded file path.
    const QString& filePath() const { return m_filePath; }

    // ── Zoom ─────────────────────────────────────────────────────────────

    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomToSelection();
    void zoomToSampleLevel();   ///< Individual samples visible as dots

    double zoomLevel() const { return m_samplesPerPixel; }

    // ── Selection ────────────────────────────────────────────────────────

    void setSelection(int64_t startSample, int64_t endSample);
    bool hasSelection() const { return m_selEnd > m_selStart; }
    int64_t selectionStart() const { return m_selStart; }
    int64_t selectionEnd()   const { return m_selEnd; }

    // ── Playback ─────────────────────────────────────────────────────────

    void play();
    void stop();
    void pause();
    bool isPlaying() const { return m_playing; }

    /// Current playback position in samples.
    int64_t playPosition() const { return m_playPosition; }

    /// Seek to a specific sample (clamped to file range).
    void seek(int64_t sample);

    /// Master output gain (linear, 0.0 - 2.0). Applied in audio callback.
    void setVolume(float gain);
    float volume() const { return m_volume.load(std::memory_order_relaxed); }

    /// Loop playback over current selection (or whole file if no selection).
    void setLooping(bool on) { m_looping = on; }
    bool isLooping() const { return m_looping; }

    /// Skip back/forward by N seconds.
    void skipSeconds(double seconds);

    /// Jump to start / end of file (or selection if active).
    void goToStart();
    void goToEnd();

    /// Step one frame at a time (1 / 30 sec) for video files.
    void stepFrame(int direction);

    // ── Analysis overlay ─────────────────────────────────────────────────

    void setOverlayMode(OverlayMode mode);
    OverlayMode overlayMode() const { return m_overlayMode; }

    /// Set detection markers (from forensic detector or spectral panel).
    void setMarkers(const QList<DetectionMarker>& markers);
    const QList<DetectionMarker>& markers() const { return m_markers; }

signals:
    /// Emitted when file is loaded successfully.
    void fileLoaded(const QString& path, int64_t frames, int channels, int sampleRate);
    /// Emitted when selection changes.
    void selectionChanged(int64_t startSample, int64_t endSample);
    /// Emitted when playback position changes.
    void positionChanged(int64_t samplePosition);
    /// Emitted when zoom level changes.
    void zoomChanged(double samplesPerPixel);
    /// Emitted when overlay analysis produces new markers.
    void markersUpdated(const QList<DetectionMarker>& markers);
    /// Emitted when playback starts/stops/pauses (true = playing).
    void playStateChanged(bool playing);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onPlaybackTimer();

private:
    // ── Coordinate mapping ───────────────────────────────────────────────
    int64_t xToSample(int x) const;
    int     sampleToX(int64_t sample) const;
    void    clampView();

    // ── Waveform drawing helpers ─────────────────────────────────────────
    void drawOverview(QPainter& p, int channel, int yCenter, int halfHeight);
    void drawMediumZoom(QPainter& p, int channel, int yCenter, int halfHeight);
    void drawSampleLevel(QPainter& p, int channel, int yCenter, int halfHeight);
    void drawTimeRuler(QPainter& p);
    void drawSelectionOverlay(QPainter& p);
    void drawPlaybackCursor(QPainter& p);
    void drawMarkers(QPainter& p);
    void drawOverlayAnalysis(QPainter& p);

    // ── PortAudio playback ───────────────────────────────────────────────
    bool initPlayback();
    void stopPlayback();
    static int paCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);

    // ── Audio data ───────────────────────────────────────────────────────
    float*  m_data       = nullptr;
    int64_t m_frames     = 0;
    int     m_channels   = 0;
    int     m_sampleRate = 44100;
    QString m_filePath;

    // ── View state ───────────────────────────────────────────────────────
    double  m_samplesPerPixel = 1.0;   ///< Zoom level (samples per pixel)
    int64_t m_viewStart       = 0;     ///< First visible sample

    // ── Selection ────────────────────────────────────────────────────────
    int64_t m_selStart = 0;
    int64_t m_selEnd   = 0;
    bool    m_selecting = false;
    int64_t m_selectAnchor = 0;

    // ── Scrolling ────────────────────────────────────────────────────────
    bool m_dragging   = false;
    int  m_dragStartX = 0;
    int64_t m_dragStartView = 0;

    // ── Playback ─────────────────────────────────────────────────────────
    bool    m_playing      = false;
    bool    m_paused       = false;
    bool    m_looping      = false;
    int64_t m_playPosition = 0;
    PaStream* m_stream     = nullptr;
    QTimer  m_playbackTimer;
    std::atomic<float> m_volume{1.0f};

    // ── Mouse mode ───────────────────────────────────────────────────────
    bool m_seekingPlayhead = false;

    // ── Overlay / analysis ───────────────────────────────────────────────
    OverlayMode m_overlayMode = OverlayMode::None;
    QList<DetectionMarker> m_markers;
};

} // namespace dawcast::editor
