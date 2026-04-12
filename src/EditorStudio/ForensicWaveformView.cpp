// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ForensicWaveformView.h"
#include "../DAWCast/codec/FFmpegCodec.h"
#include "../DAWCast/dsp/FFT.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QApplication>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

namespace dawcast::editor {

ForensicWaveformView::ForensicWaveformView(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_playbackTimer.setInterval(30); // ~33 fps cursor update
    connect(&m_playbackTimer, &QTimer::timeout,
            this, &ForensicWaveformView::onPlaybackTimer);
}

ForensicWaveformView::~ForensicWaveformView()
{
    stopPlayback();
    delete[] m_data;
}

// ── File loading ───────────────────────────────────────────────────────────

void ForensicWaveformView::loadFile(const QString& path)
{
    stopPlayback();
    delete[] m_data;
    m_data = nullptr;
    m_frames = 0;
    m_channels = 0;
    m_selStart = 0;
    m_selEnd = 0;
    m_viewStart = 0;
    m_playPosition = 0;
    m_markers.clear();
    m_filePath = path;

    // Decode via FFmpegCodec
    dawcast::FFmpegCodec codec;
    dawcast::AudioBuffer buf = codec.decode(path);

    if (!buf.data || buf.frames <= 0) {
        qWarning() << "ForensicWaveformView: failed to decode" << path;
        m_filePath.clear();
        update();
        return;
    }

    m_data       = buf.data;
    m_frames     = buf.frames;
    m_channels   = buf.channels;
    m_sampleRate = buf.sampleRate;

    // NOTE: ownership of buf.data transferred to m_data (do not delete buf.data)

    zoomToFit();
    emit fileLoaded(path, m_frames, m_channels, m_sampleRate);
    update();
}

// ── Zoom ───────────────────────────────────────────────────────────────────

void ForensicWaveformView::zoomIn()
{
    // Zoom centered on view midpoint
    int64_t centerSample = m_viewStart +
        static_cast<int64_t>(width() / 2 * m_samplesPerPixel);

    m_samplesPerPixel *= 0.5;
    if (m_samplesPerPixel < 0.05) m_samplesPerPixel = 0.05; // extreme sample-level

    m_viewStart = centerSample -
        static_cast<int64_t>(width() / 2 * m_samplesPerPixel);
    clampView();
    emit zoomChanged(m_samplesPerPixel);
    update();
}

void ForensicWaveformView::zoomOut()
{
    int64_t centerSample = m_viewStart +
        static_cast<int64_t>(width() / 2 * m_samplesPerPixel);

    m_samplesPerPixel *= 2.0;
    double maxSPP = (m_frames > 0) ?
        static_cast<double>(m_frames) / static_cast<double>(width()) : 1.0;
    if (m_samplesPerPixel > maxSPP) m_samplesPerPixel = maxSPP;

    m_viewStart = centerSample -
        static_cast<int64_t>(width() / 2 * m_samplesPerPixel);
    clampView();
    emit zoomChanged(m_samplesPerPixel);
    update();
}

void ForensicWaveformView::zoomToFit()
{
    if (m_frames <= 0 || width() <= 0) return;
    m_samplesPerPixel = static_cast<double>(m_frames) / static_cast<double>(width());
    m_viewStart = 0;
    emit zoomChanged(m_samplesPerPixel);
    update();
}

void ForensicWaveformView::zoomToSelection()
{
    if (!hasSelection() || width() <= 0) return;
    int64_t selLen = m_selEnd - m_selStart;
    m_samplesPerPixel = static_cast<double>(selLen) / static_cast<double>(width());
    if (m_samplesPerPixel < 0.05) m_samplesPerPixel = 0.05;
    m_viewStart = m_selStart;
    clampView();
    emit zoomChanged(m_samplesPerPixel);
    update();
}

void ForensicWaveformView::zoomToSampleLevel()
{
    m_samplesPerPixel = 0.1; // ~10 pixels per sample
    int64_t centerSample = m_viewStart +
        static_cast<int64_t>(width() / 2 * m_samplesPerPixel * 10);
    m_viewStart = centerSample -
        static_cast<int64_t>(width() / 2 * m_samplesPerPixel);
    clampView();
    emit zoomChanged(m_samplesPerPixel);
    update();
}

// ── Selection ──────────────────────────────────────────────────────────────

void ForensicWaveformView::setSelection(int64_t startSample, int64_t endSample)
{
    m_selStart = std::max(static_cast<int64_t>(0), std::min(startSample, endSample));
    m_selEnd   = std::min(m_frames, std::max(startSample, endSample));
    emit selectionChanged(m_selStart, m_selEnd);
    update();
}

// ── Playback ───────────────────────────────────────────────────────────────

void ForensicWaveformView::play()
{
    if (!m_data || m_frames <= 0) return;

    if (m_paused && m_stream) {
        m_paused = false;
        m_playing = true;
#ifdef HAVE_PORTAUDIO
        Pa_StartStream(m_stream);
#endif
        m_playbackTimer.start();
        emit playStateChanged(true);
        return;
    }

    if (m_playing) stop();

    // Resume from current position; only jump to selection start if at file end
    if (m_playPosition >= m_frames) {
        m_playPosition = hasSelection() ? m_selStart : 0;
    }

    if (initPlayback()) {
        m_playing = true;
        m_paused = false;
        m_playbackTimer.start();
        emit playStateChanged(true);
    }
}

void ForensicWaveformView::stop()
{
    bool wasPlaying = m_playing || m_paused;
    stopPlayback();
    m_playPosition = hasSelection() ? m_selStart : 0;
    emit positionChanged(m_playPosition);
    if (wasPlaying) emit playStateChanged(false);
    update();
}

void ForensicWaveformView::pause()
{
    if (!m_playing) return;
    m_playing = false;
    m_paused = true;
#ifdef HAVE_PORTAUDIO
    if (m_stream) Pa_StopStream(m_stream);
#endif
    m_playbackTimer.stop();
    emit playStateChanged(false);
    update();
}

void ForensicWaveformView::seek(int64_t sample)
{
    if (m_frames <= 0) return;
    sample = std::max(static_cast<int64_t>(0), std::min(sample, m_frames - 1));
    m_playPosition = sample;
    emit positionChanged(m_playPosition);
    update();
}

void ForensicWaveformView::setVolume(float gain)
{
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 4.0f) gain = 4.0f;
    m_volume.store(gain, std::memory_order_relaxed);
}

void ForensicWaveformView::skipSeconds(double seconds)
{
    if (m_sampleRate <= 0) return;
    int64_t delta = static_cast<int64_t>(seconds * m_sampleRate);
    seek(m_playPosition + delta);
}

void ForensicWaveformView::goToStart()
{
    seek(hasSelection() ? m_selStart : 0);
}

void ForensicWaveformView::goToEnd()
{
    seek(hasSelection() ? m_selEnd : m_frames - 1);
}

void ForensicWaveformView::stepFrame(int direction)
{
    if (m_sampleRate <= 0) return;
    // Assume 30fps step (1/30 sec)
    int64_t delta = static_cast<int64_t>(m_sampleRate / 30) * direction;
    seek(m_playPosition + delta);
}

bool ForensicWaveformView::initPlayback()
{
#ifdef HAVE_PORTAUDIO
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qWarning() << "ForensicWaveformView: Pa_Initialize failed:" << Pa_GetErrorText(err);
        return false;
    }

    PaStreamParameters params;
    params.device = Pa_GetDefaultOutputDevice();
    if (params.device == paNoDevice) {
        qWarning() << "ForensicWaveformView: no output device";
        Pa_Terminate();
        return false;
    }
    params.channelCount = m_channels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&m_stream, nullptr, &params,
                        m_sampleRate, 256, paClipOff,
                        &ForensicWaveformView::paCallback, this);
    if (err != paNoError) {
        qWarning() << "ForensicWaveformView: Pa_OpenStream failed:" << Pa_GetErrorText(err);
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        qWarning() << "ForensicWaveformView: Pa_StartStream failed:" << Pa_GetErrorText(err);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        Pa_Terminate();
        return false;
    }

    return true;
#else
    qWarning() << "ForensicWaveformView: PortAudio not available";
    return false;
#endif
}

void ForensicWaveformView::stopPlayback()
{
    m_playing = false;
    m_paused = false;
    m_playbackTimer.stop();

#ifdef HAVE_PORTAUDIO
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        Pa_Terminate();
    }
#endif
}

int ForensicWaveformView::paCallback(const void* /*input*/, void* output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                      PaStreamCallbackFlags /*statusFlags*/,
                                      void* userData)
{
    auto* self = static_cast<ForensicWaveformView*>(userData);
    auto* out = static_cast<float*>(output);
    const float gain = self->m_volume.load(std::memory_order_relaxed);

    int64_t startPosition = 0;
    int64_t endPosition = self->m_frames;
    if (self->hasSelection()) {
        startPosition = self->m_selStart;
        endPosition = self->m_selEnd;
    }

    int64_t pos = self->m_playPosition;
    const int channels = self->m_channels;
    const bool looping = self->m_looping;
    unsigned long written = 0;

    while (written < frameCount) {
        if (pos >= endPosition) {
            if (looping) {
                pos = startPosition;
            } else {
                break;
            }
        }
        for (int ch = 0; ch < channels; ++ch) {
            out[written * static_cast<unsigned long>(channels) + static_cast<unsigned long>(ch)] =
                self->m_data[pos * channels + ch] * gain;
        }
        ++pos;
        ++written;
    }

    // Zero-fill remainder
    unsigned long remaining = frameCount - written;
    if (remaining > 0) {
        std::memset(out + written * static_cast<unsigned long>(channels),
                    0,
                    remaining * static_cast<unsigned long>(channels) * sizeof(float));
    }

    self->m_playPosition = pos;

    if (pos >= endPosition && !looping) {
        return 1; // paComplete
    }
    return 0; // paContinue
}

void ForensicWaveformView::onPlaybackTimer()
{
    if (!m_playing) {
        m_playbackTimer.stop();
        return;
    }

    int64_t endPos = hasSelection() ? m_selEnd : m_frames;
    if (m_playPosition >= endPos) {
        stopPlayback();
    }

    emit positionChanged(m_playPosition);
    update();
}

// ── Overlay mode ───────────────────────────────────────────────────────────

void ForensicWaveformView::setOverlayMode(OverlayMode mode)
{
    m_overlayMode = mode;

    if (mode == OverlayMode::None) {
        // Clear overlay markers only (keep user markers)
        update();
        return;
    }

    if (!m_data || m_frames <= 0) return;

    ForensicDetector detector;
    detector.setAudioData(m_data, m_frames, m_channels, m_sampleRate);

    QList<DetectionMarker> newMarkers;

    switch (mode) {
    case OverlayMode::FrequencyDetect:
        newMarkers = detector.detectFrequencyRange(200.0, 4000.0, -40.0);
        break;
    case OverlayMode::AnomalyScan:
        newMarkers = detector.detectAnomalies(70.0);
        break;
    case OverlayMode::EVPDetection:
        newMarkers = detector.detectEVP(70.0);
        break;
    case OverlayMode::InfrasonicScan:
        newMarkers = detector.detectInfrasonic();
        break;
    case OverlayMode::UltrasonicScan:
        newMarkers = detector.detectUltrasonic();
        break;
    case OverlayMode::PhaseAnalysis:
        newMarkers = detector.detectPhaseAnomalies();
        break;
    case OverlayMode::ParanormalMode:
        // Combined: run all detectors
        newMarkers.append(detector.detectInfrasonic());
        newMarkers.append(detector.detectUltrasonic());
        newMarkers.append(detector.detectAnomalies(80.0));
        newMarkers.append(detector.detectEVP(80.0));
        newMarkers.append(detector.detectPhaseAnomalies());
        break;
    default:
        break;
    }

    m_markers = newMarkers;
    emit markersUpdated(m_markers);
    update();
}

void ForensicWaveformView::setMarkers(const QList<DetectionMarker>& markers)
{
    m_markers = markers;
    update();
}

// ── Coordinate mapping ─────────────────────────────────────────────────────

int64_t ForensicWaveformView::xToSample(int x) const
{
    return m_viewStart + static_cast<int64_t>(x * m_samplesPerPixel);
}

int ForensicWaveformView::sampleToX(int64_t sample) const
{
    if (m_samplesPerPixel <= 0.0) return 0;
    return static_cast<int>(
        static_cast<double>(sample - m_viewStart) / m_samplesPerPixel);
}

void ForensicWaveformView::clampView()
{
    if (m_viewStart < 0) m_viewStart = 0;
    int64_t maxStart = m_frames -
        static_cast<int64_t>(width() * m_samplesPerPixel);
    if (maxStart < 0) maxStart = 0;
    if (m_viewStart > maxStart) m_viewStart = maxStart;
}

// ── Paint ──────────────────────────────────────────────────────────────────

void ForensicWaveformView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor(32, 32, 38));

    if (!m_data || m_frames <= 0) {
        p.setPen(QColor(120, 120, 120));
        p.setFont(QFont(QStringLiteral("Helvetica"), 14));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("No file loaded\n\nDrag & drop or use File > Open"));
        return;
    }

    int h = height();
    int rulerHeight = 24;
    int waveHeight = h - rulerHeight;
    int channelHeight = waveHeight / std::max(1, m_channels);

    // Draw selection first (behind waveform)
    if (hasSelection()) {
        drawSelectionOverlay(p);
    }

    // Draw center lines and waveforms per channel
    for (int ch = 0; ch < m_channels; ++ch) {
        int yCenter = rulerHeight + ch * channelHeight + channelHeight / 2;
        int halfH = channelHeight / 2 - 4;

        // Center line
        p.setPen(QPen(QColor(60, 60, 70), 1));
        p.drawLine(0, yCenter, width(), yCenter);

        // Determine zoom level and choose rendering method
        if (m_samplesPerPixel > 8.0) {
            drawOverview(p, ch, yCenter, halfH);
        } else if (m_samplesPerPixel > 0.2) {
            drawMediumZoom(p, ch, yCenter, halfH);
        } else {
            drawSampleLevel(p, ch, yCenter, halfH);
        }
    }

    // Overlay analysis
    if (m_overlayMode != OverlayMode::None) {
        drawOverlayAnalysis(p);
    }

    // Detection markers
    drawMarkers(p);

    // Playback cursor
    drawPlaybackCursor(p);

    // Time ruler
    drawTimeRuler(p);
}

// ── Waveform drawing: overview (peak/RMS bars) ────────────────────────────

void ForensicWaveformView::drawOverview(QPainter& p, int channel,
                                         int yCenter, int halfHeight)
{
    int w = width();
    QColor peakColor(80, 180, 80);
    QColor rmsColor(40, 120, 40);

    for (int x = 0; x < w; ++x) {
        int64_t sStart = xToSample(x);
        int64_t sEnd   = xToSample(x + 1);
        sStart = std::max(static_cast<int64_t>(0), sStart);
        sEnd   = std::min(m_frames, sEnd);
        if (sStart >= sEnd) continue;

        float peak = 0.0f;
        double rmsSum = 0.0;
        int count = 0;

        for (int64_t s = sStart; s < sEnd; ++s) {
            float val = m_data[s * m_channels + channel];
            float absVal = std::fabs(val);
            if (absVal > peak) peak = absVal;
            rmsSum += static_cast<double>(val) * static_cast<double>(val);
            ++count;
        }

        float rms = (count > 0) ?
            static_cast<float>(std::sqrt(rmsSum / count)) : 0.0f;

        int peakH = static_cast<int>(peak * halfHeight);
        int rmsH  = static_cast<int>(rms * halfHeight);

        // Draw peak bar
        p.setPen(peakColor);
        p.drawLine(x, yCenter - peakH, x, yCenter + peakH);

        // Draw RMS overlay (brighter)
        p.setPen(rmsColor);
        p.drawLine(x, yCenter - rmsH, x, yCenter + rmsH);
    }
}

// ── Waveform drawing: medium zoom (waveform outline) ──────────────────────

void ForensicWaveformView::drawMediumZoom(QPainter& p, int channel,
                                           int yCenter, int halfHeight)
{
    int w = width();
    QPen wavePen(QColor(60, 200, 60), 1);
    p.setPen(wavePen);

    QVector<QPointF> points;
    points.reserve(w);

    for (int x = 0; x < w; ++x) {
        int64_t s = xToSample(x);
        if (s < 0 || s >= m_frames) continue;

        float val = m_data[s * m_channels + channel];
        int y = yCenter - static_cast<int>(val * halfHeight);
        points.append(QPointF(x, y));
    }

    if (points.size() > 1) {
        p.drawPolyline(points.data(), points.size());
    }
}

// ── Waveform drawing: sample level (individual dots + values) ─────────────

void ForensicWaveformView::drawSampleLevel(QPainter& p, int channel,
                                            int yCenter, int halfHeight)
{
    int w = width();
    QColor dotColor(100, 255, 100);
    QColor lineColor(60, 200, 60, 120);
    QFont smallFont(QStringLiteral("Monospace"), 7);
    p.setFont(smallFont);

    QVector<QPointF> points;

    for (int x = 0; x < w; ++x) {
        int64_t s = xToSample(x);
        if (s < 0 || s >= m_frames) continue;

        float val = m_data[s * m_channels + channel];
        int y = yCenter - static_cast<int>(val * halfHeight);
        points.append(QPointF(x, y));
    }

    // Draw interpolation lines
    p.setPen(QPen(lineColor, 1));
    if (points.size() > 1) {
        p.drawPolyline(points.data(), points.size());
    }

    // Draw sample dots
    p.setPen(Qt::NoPen);
    p.setBrush(dotColor);
    for (const auto& pt : points) {
        p.drawEllipse(pt, 3, 3);
    }

    // Draw sample values if zoomed in enough
    if (m_samplesPerPixel < 0.15) {
        p.setPen(QColor(200, 200, 200));
        for (int x = 0; x < w; ++x) {
            int64_t s = xToSample(x);
            if (s < 0 || s >= m_frames) continue;
            float val = m_data[s * m_channels + channel];
            int y = yCenter - static_cast<int>(val * halfHeight);
            p.drawText(x + 5, y - 5,
                       QString::number(static_cast<double>(val), 'f', 4));
        }
    }
}

// ── Time ruler ─────────────────────────────────────────────────────────────

void ForensicWaveformView::drawTimeRuler(QPainter& p)
{
    int rulerH = 24;
    p.fillRect(0, 0, width(), rulerH, QColor(50, 50, 58));

    p.setPen(QColor(180, 180, 180));
    p.setFont(QFont(QStringLiteral("Monospace"), 8));

    // Choose time interval based on zoom
    double secondsPerPixel = m_samplesPerPixel / static_cast<double>(m_sampleRate);
    double totalVisible = secondsPerPixel * width();

    // Adaptive tick interval
    double interval = 1.0;
    if (totalVisible > 3600) interval = 600.0;
    else if (totalVisible > 600) interval = 60.0;
    else if (totalVisible > 60) interval = 10.0;
    else if (totalVisible > 10) interval = 1.0;
    else if (totalVisible > 1) interval = 0.1;
    else if (totalVisible > 0.1) interval = 0.01;
    else interval = 0.001;

    double startTime = static_cast<double>(m_viewStart) /
        static_cast<double>(m_sampleRate);
    double firstTick = std::ceil(startTime / interval) * interval;

    for (double t = firstTick; ; t += interval) {
        int64_t sample = static_cast<int64_t>(t * m_sampleRate);
        int x = sampleToX(sample);
        if (x > width()) break;
        if (x < 0) continue;

        p.drawLine(x, rulerH - 6, x, rulerH);

        // Format time label
        QString label;
        if (interval >= 60.0) {
            int mins = static_cast<int>(t) / 60;
            int secs = static_cast<int>(t) % 60;
            label = QString("%1:%2").arg(mins).arg(secs, 2, 10, QLatin1Char('0'));
        } else if (interval >= 1.0) {
            label = QString("%1s").arg(t, 0, 'f', 0);
        } else if (interval >= 0.01) {
            label = QString("%1s").arg(t, 0, 'f', 2);
        } else {
            label = QString("%1ms").arg(t * 1000.0, 0, 'f', 1);
        }

        p.drawText(x + 2, rulerH - 8, label);
    }
}

// ── Selection overlay ──────────────────────────────────────────────────────

void ForensicWaveformView::drawSelectionOverlay(QPainter& p)
{
    int x1 = sampleToX(m_selStart);
    int x2 = sampleToX(m_selEnd);
    if (x1 > x2) std::swap(x1, x2);

    QColor selColor(65, 105, 225, 60); // royal blue, translucent
    p.fillRect(x1, 24, x2 - x1, height() - 24, selColor);

    // Selection edges
    p.setPen(QPen(QColor(65, 105, 225, 180), 1));
    p.drawLine(x1, 24, x1, height());
    p.drawLine(x2, 24, x2, height());
}

// ── Playback cursor ────────────────────────────────────────────────────────

void ForensicWaveformView::drawPlaybackCursor(QPainter& p)
{
    if (m_playPosition <= 0 && !m_playing) return;

    int cx = sampleToX(m_playPosition);
    if (cx < 0 || cx > width()) return;

    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(cx, 24, cx, height());

    // Triangle at top
    QPolygonF tri;
    tri << QPointF(cx - 5, 24) << QPointF(cx + 5, 24) << QPointF(cx, 30);
    p.setBrush(QColor(255, 80, 80));
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
}

// ── Detection markers ──────────────────────────────────────────────────────

void ForensicWaveformView::drawMarkers(QPainter& p)
{
    p.setFont(QFont(QStringLiteral("Monospace"), 7));

    for (const auto& marker : m_markers) {
        if (marker.frameIndex >= 0) continue; // video markers not shown on waveform

        int mx = sampleToX(marker.positionSamples);
        if (mx < 0 || mx > width()) continue;

        // Marker line
        p.setPen(QPen(marker.color, 1, Qt::DashLine));
        p.drawLine(mx, 24, mx, height());

        // Small triangle at top
        QPolygonF tri;
        tri << QPointF(mx - 4, 25) << QPointF(mx + 4, 25) << QPointF(mx, 31);
        p.setBrush(marker.color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(tri);
    }
}

// ── Overlay analysis visualization ─────────────────────────────────────────

void ForensicWaveformView::drawOverlayAnalysis(QPainter& p)
{
    // Semi-transparent colored overlays based on mode
    QColor overlayColor;
    switch (m_overlayMode) {
    case OverlayMode::SpectralView:     overlayColor = QColor(0, 100, 255, 30); break;
    case OverlayMode::FrequencyDetect:  overlayColor = QColor(255, 165, 0, 20); break;
    case OverlayMode::PhaseAnalysis:    overlayColor = QColor(0, 200, 200, 20); break;
    case OverlayMode::AnomalyScan:      overlayColor = QColor(255, 0, 0, 20); break;
    case OverlayMode::EVPDetection:     overlayColor = QColor(148, 0, 211, 20); break;
    case OverlayMode::InfrasonicScan:   overlayColor = QColor(0, 0, 180, 20); break;
    case OverlayMode::UltrasonicScan:   overlayColor = QColor(255, 0, 255, 20); break;
    case OverlayMode::ParanormalMode:   overlayColor = QColor(0, 255, 0, 15); break;
    default: return;
    }

    // Draw subtle tinted overlay
    p.fillRect(0, 24, width(), height() - 24, overlayColor);

    // Mode label
    static const char* modeNames[] = {
        "", "Spectral View", "Frequency Detect", "Phase Analysis",
        "Anomaly Scan", "EVP Detection", "Infrasonic Scan",
        "Ultrasonic Scan", "PARANORMAL MODE", "Orb Detection"
    };
    int idx = static_cast<int>(m_overlayMode);
    if (idx > 0 && idx < 10) {
        p.setPen(overlayColor.lighter(300));
        p.setFont(QFont(QStringLiteral("Helvetica"), 10, QFont::Bold));
        p.drawText(10, height() - 10, QString::fromLatin1(modeNames[idx]));
    }
}

// ── Mouse handling ─────────────────────────────────────────────────────────

void ForensicWaveformView::mousePressEvent(QMouseEvent* event)
{
    if (!m_data) return;

    const int rulerHeight = 24;
    const int x = event->pos().x();
    const int y = event->pos().y();

    if (event->button() == Qt::LeftButton) {
        // Click in time ruler OR plain click anywhere = seek/scrub the playhead
        // Shift+click in waveform = make/extend selection
        // Cmd/Ctrl+click+drag in waveform = pan view
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift+click: start (or extend) selection
            int64_t clickSample = xToSample(x);
            if (hasSelection()) {
                if (clickSample < m_selStart) {
                    m_selStart = clickSample;
                } else {
                    m_selEnd = clickSample;
                }
            } else {
                m_selecting = true;
                m_selectAnchor = clickSample;
                m_selStart = clickSample;
                m_selEnd = clickSample;
            }
            emit selectionChanged(m_selStart, m_selEnd);
            update();
        } else if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) {
            // Cmd/Ctrl+drag: pan view
            m_dragging = true;
            m_dragStartX = x;
            m_dragStartView = m_viewStart;
            setCursor(Qt::ClosedHandCursor);
        } else {
            // Plain left-click anywhere: scrub playhead. Drag continues to scrub.
            m_seekingPlayhead = true;
            // Clear any selection — user is just seeking
            if (y < rulerHeight) {
                m_selStart = m_selEnd = 0;
                emit selectionChanged(m_selStart, m_selEnd);
            }
            seek(xToSample(x));
        }
    } else if (event->button() == Qt::MiddleButton) {
        // Middle-click: pan view
        m_dragging = true;
        m_dragStartX = x;
        m_dragStartView = m_viewStart;
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::RightButton) {
        // Right-click also seeks
        seek(xToSample(x));
    }
}

void ForensicWaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_seekingPlayhead) {
        seek(xToSample(event->pos().x()));
    } else if (m_selecting) {
        int64_t current = xToSample(event->pos().x());
        m_selStart = std::min(m_selectAnchor, current);
        m_selEnd   = std::max(m_selectAnchor, current);
        m_selStart = std::max(static_cast<int64_t>(0), m_selStart);
        m_selEnd   = std::min(m_frames, m_selEnd);
        emit selectionChanged(m_selStart, m_selEnd);
        update();
    } else if (m_dragging) {
        int dx = event->pos().x() - m_dragStartX;
        m_viewStart = m_dragStartView -
            static_cast<int64_t>(dx * m_samplesPerPixel);
        clampView();
        update();
    }
}

void ForensicWaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = false;
        m_seekingPlayhead = false;
        if (m_dragging) {
            m_dragging = false;
            setCursor(Qt::ArrowCursor);
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ForensicWaveformView::wheelEvent(QWheelEvent* event)
{
    if (!m_data) return;

    // Zoom centered on mouse position
    int mouseX = event->position().toPoint().x();
    int64_t centerSample = xToSample(mouseX);

    double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
    m_samplesPerPixel *= factor;

    // Clamp zoom
    double maxSPP = static_cast<double>(m_frames) / static_cast<double>(width());
    if (m_samplesPerPixel > maxSPP) m_samplesPerPixel = maxSPP;
    if (m_samplesPerPixel < 0.05) m_samplesPerPixel = 0.05;

    // Keep the sample under the mouse at the same pixel position
    m_viewStart = centerSample -
        static_cast<int64_t>(mouseX * m_samplesPerPixel);
    clampView();

    emit zoomChanged(m_samplesPerPixel);
    update();
}

void ForensicWaveformView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    clampView();
}

void ForensicWaveformView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        if (m_playing) pause();
        else play();
        break;
    case Qt::Key_Home:
        m_playPosition = 0;
        m_viewStart = 0;
        emit positionChanged(m_playPosition);
        update();
        break;
    case Qt::Key_End:
        m_playPosition = m_frames;
        emit positionChanged(m_playPosition);
        update();
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        break;
    case Qt::Key_Minus:
        zoomOut();
        break;
    case Qt::Key_A:
        if (event->modifiers() & Qt::ControlModifier) {
            setSelection(0, m_frames);
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

} // namespace dawcast::editor
