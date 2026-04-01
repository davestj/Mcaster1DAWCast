// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ForensicDetector.h"
#include "../DAWCast/dsp/FFT.h"

#include <QDebug>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace dawcast::editor {

ForensicDetector::ForensicDetector(QObject* parent)
    : QObject(parent)
{
}

ForensicDetector::~ForensicDetector() = default;

void ForensicDetector::setAudioData(const float* data, int64_t frames,
                                     int channels, int sampleRate)
{
    m_data       = data;
    m_frames     = frames;
    m_channels   = channels;
    m_sampleRate = sampleRate;
    m_monoValid  = false;
    m_mono.clear();
}

// ── Private helpers ────────────────────────────────────────────────────────

void ForensicDetector::mixToMono(std::vector<float>& mono)
{
    if (m_monoValid) {
        mono = m_mono;
        return;
    }
    mono.resize(static_cast<size_t>(m_frames));
    if (m_channels == 1) {
        std::copy(m_data, m_data + m_frames, mono.begin());
    } else {
        for (int64_t i = 0; i < m_frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < m_channels; ++ch) {
                sum += m_data[i * m_channels + ch];
            }
            mono[static_cast<size_t>(i)] = sum / static_cast<float>(m_channels);
        }
    }
    m_mono = mono;
    m_monoValid = true;
}

void ForensicDetector::computeSpectrum(int64_t sampleOffset, int fftSize,
                                        std::vector<float>& magnitudesDb)
{
    std::vector<float> mono;
    mixToMono(mono);

    magnitudesDb.resize(static_cast<size_t>(fftSize / 2));

    // Extract windowed block
    std::vector<float> block(static_cast<size_t>(fftSize), 0.0f);
    int64_t available = std::min(static_cast<int64_t>(fftSize),
                                 m_frames - sampleOffset);
    if (available <= 0) {
        std::fill(magnitudesDb.begin(), magnitudesDb.end(), -120.0f);
        return;
    }

    for (int64_t i = 0; i < available; ++i) {
        // Hann window
        double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(i)
                                          / static_cast<double>(fftSize - 1)));
        block[static_cast<size_t>(i)] =
            mono[static_cast<size_t>(sampleOffset + i)] * static_cast<float>(w);
    }

    // Run FFT
    dawcast::FFT fft(fftSize);
    std::vector<float> real(static_cast<size_t>(fftSize));
    std::vector<float> imag(static_cast<size_t>(fftSize));
    fft.forward(block.data(), real.data(), imag.data());

    // Compute magnitude in dB
    int halfSize = fftSize / 2;
    for (int i = 0; i < halfSize; ++i) {
        float re = real[static_cast<size_t>(i)];
        float im = imag[static_cast<size_t>(i)];
        float mag = std::sqrt(re * re + im * im) / static_cast<float>(fftSize);
        if (mag < 1e-10f) mag = 1e-10f;
        magnitudesDb[static_cast<size_t>(i)] = 20.0f * std::log10(mag);
    }
}

double ForensicDetector::computeRMS(const float* monoData, int64_t frames)
{
    if (frames <= 0) return 0.0;
    double sum = 0.0;
    for (int64_t i = 0; i < frames; ++i) {
        double s = static_cast<double>(monoData[i]);
        sum += s * s;
    }
    return std::sqrt(sum / static_cast<double>(frames));
}

// ── Frequency range detection ──────────────────────────────────────────────

QList<DetectionMarker> ForensicDetector::detectFrequencyRange(
    double minHz, double maxHz, double thresholdDb)
{
    QList<DetectionMarker> markers;
    if (!m_data || m_frames <= 0) return markers;

    const int fftSize = 4096;
    const int hopSize = fftSize / 2;
    double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(fftSize);
    int minBin = std::max(1, static_cast<int>(std::floor(minHz / binHz)));
    int maxBin = std::min(fftSize / 2 - 1,
                          static_cast<int>(std::ceil(maxHz / binHz)));

    std::vector<float> magDb;
    int64_t totalSteps = (m_frames - fftSize) / hopSize;
    int64_t step = 0;

    for (int64_t offset = 0; offset + fftSize <= m_frames; offset += hopSize) {
        computeSpectrum(offset, fftSize, magDb);

        // Find peak in target range
        int peakBin = minBin;
        float peakMag = magDb[static_cast<size_t>(minBin)];
        for (int b = minBin + 1; b <= maxBin; ++b) {
            if (magDb[static_cast<size_t>(b)] > peakMag) {
                peakMag = magDb[static_cast<size_t>(b)];
                peakBin = b;
            }
        }

        if (peakMag >= thresholdDb) {
            DetectionMarker m;
            m.positionSamples = offset;
            m.frequencyHz     = static_cast<double>(peakBin) * binHz;
            m.magnitudeDb     = peakMag;
            m.label = QString("Freq %1 Hz @ %2 dB")
                          .arg(m.frequencyHz, 0, 'f', 1)
                          .arg(m.magnitudeDb, 0, 'f', 1);
            m.color = QColor(255, 165, 0); // orange
            markers.append(m);
        }

        ++step;
        if (totalSteps > 0) {
            emit progress(static_cast<int>(step * 100 / totalSteps));
        }
    }

    emit detectionComplete(markers);
    return markers;
}

// ── Anomaly detection ──────────────────────────────────────────────────────

QList<DetectionMarker> ForensicDetector::detectAnomalies(double sensitivityPercent)
{
    QList<DetectionMarker> markers;
    if (!m_data || m_frames <= 0) return markers;

    const int fftSize = 4096;
    const int hopSize = fftSize / 2;
    double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(fftSize);

    // Sensitivity maps to threshold: high sensitivity = lower threshold
    double sensitivityFactor = 1.0 - (sensitivityPercent / 100.0);
    double anomalyThresholdDb = -60.0 + sensitivityFactor * 40.0; // -60 to -20 dB

    // First pass: compute average spectrum
    std::vector<double> avgSpectrum(static_cast<size_t>(fftSize / 2), 0.0);
    int windowCount = 0;
    std::vector<float> magDb;

    for (int64_t offset = 0; offset + fftSize <= m_frames; offset += hopSize) {
        computeSpectrum(offset, fftSize, magDb);
        for (int b = 0; b < fftSize / 2; ++b) {
            avgSpectrum[static_cast<size_t>(b)] +=
                static_cast<double>(magDb[static_cast<size_t>(b)]);
        }
        ++windowCount;
    }
    if (windowCount == 0) return markers;

    for (auto& v : avgSpectrum) {
        v /= static_cast<double>(windowCount);
    }

    // Second pass: find windows that deviate significantly from average
    int64_t step = 0;
    int64_t totalSteps = (m_frames - fftSize) / hopSize;

    for (int64_t offset = 0; offset + fftSize <= m_frames; offset += hopSize) {
        computeSpectrum(offset, fftSize, magDb);

        // Compute deviation from average spectrum
        double maxDeviation = 0.0;
        int deviationBin = 0;
        for (int b = 1; b < fftSize / 2; ++b) {
            double dev = static_cast<double>(magDb[static_cast<size_t>(b)])
                       - avgSpectrum[static_cast<size_t>(b)];
            if (dev > maxDeviation) {
                maxDeviation = dev;
                deviationBin = b;
            }
        }

        // Anomaly if deviation exceeds threshold
        double deviationThreshold = 12.0 + sensitivityFactor * 18.0; // 12-30 dB above average
        if (maxDeviation > deviationThreshold &&
            magDb[static_cast<size_t>(deviationBin)] > anomalyThresholdDb) {
            DetectionMarker m;
            m.positionSamples = offset;
            m.frequencyHz     = static_cast<double>(deviationBin) * binHz;
            m.magnitudeDb     = magDb[static_cast<size_t>(deviationBin)];
            m.label = QString("Anomaly: +%1 dB deviation @ %2 Hz")
                          .arg(maxDeviation, 0, 'f', 1)
                          .arg(m.frequencyHz, 0, 'f', 1);
            m.color = QColor(255, 0, 0); // red
            markers.append(m);
        }

        ++step;
        if (totalSteps > 0) {
            emit progress(static_cast<int>(step * 100 / totalSteps));
        }
    }

    emit detectionComplete(markers);
    return markers;
}

// ── EVP Detection (Electronic Voice Phenomena) ─────────────────────────────

QList<DetectionMarker> ForensicDetector::detectEVP(double sensitivity)
{
    QList<DetectionMarker> markers;
    if (!m_data || m_frames <= 0) return markers;

    const int fftSize = 4096;
    const int hopSize = fftSize / 4; // 75% overlap for finer resolution
    double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(fftSize);

    // EVP frequency range: 200Hz - 4kHz (human voice formants)
    int minBin = std::max(1, static_cast<int>(std::floor(200.0 / binHz)));
    int maxBin = std::min(fftSize / 2 - 1,
                          static_cast<int>(std::ceil(4000.0 / binHz)));

    std::vector<float> mono;
    mixToMono(mono);

    // Compute overall RMS to identify "quiet" regions
    double overallRMS = computeRMS(mono.data(), m_frames);
    double quietThreshold = overallRMS * (0.1 + 0.2 * (1.0 - sensitivity / 100.0));

    std::vector<float> magDb;
    int64_t step = 0;
    int64_t totalSteps = (m_frames - fftSize) / hopSize;

    for (int64_t offset = 0; offset + fftSize <= m_frames; offset += hopSize) {
        // Check if this region is "quiet" (low overall energy)
        int64_t rmsFrames = std::min(static_cast<int64_t>(fftSize), m_frames - offset);
        double localRMS = computeRMS(mono.data() + offset, rmsFrames);

        if (localRMS < quietThreshold) {
            computeSpectrum(offset, fftSize, magDb);

            // Look for formant-like peaks in the voice range
            // Voice formants: clusters of energy at specific frequencies
            int peakCount = 0;
            float peakEnergy = -120.0f;
            int bestBin = minBin;

            for (int b = minBin; b <= maxBin; ++b) {
                float mag = magDb[static_cast<size_t>(b)];
                // A formant peak: higher than neighbors
                if (b > minBin && b < maxBin) {
                    float prev = magDb[static_cast<size_t>(b - 1)];
                    float next = magDb[static_cast<size_t>(b + 1)];
                    if (mag > prev && mag > next && mag > -80.0f) {
                        ++peakCount;
                        if (mag > peakEnergy) {
                            peakEnergy = mag;
                            bestBin = b;
                        }
                    }
                }
            }

            // EVP marker if we find formant-like structure in a quiet region
            // (2+ peaks suggest vocal formant structure)
            if (peakCount >= 2 && peakEnergy > -70.0f) {
                DetectionMarker m;
                m.positionSamples = offset;
                m.frequencyHz     = static_cast<double>(bestBin) * binHz;
                m.magnitudeDb     = peakEnergy;
                m.label = QString("EVP: %1 formant peaks @ %2 Hz")
                              .arg(peakCount)
                              .arg(m.frequencyHz, 0, 'f', 0);
                m.color = QColor(148, 0, 211); // violet
                markers.append(m);
            }
        }

        ++step;
        if (totalSteps > 0) {
            emit progress(static_cast<int>(step * 100 / totalSteps));
        }
    }

    emit detectionComplete(markers);
    return markers;
}

// ── Infrasonic Detection (0.5Hz - 20Hz) ────────────────────────────────────

QList<DetectionMarker> ForensicDetector::detectInfrasonic()
{
    QList<DetectionMarker> markers;
    if (!m_data || m_frames <= 0) return markers;

    // Large FFT for sub-bass frequency resolution
    const int fftSize = 65536;
    const int hopSize = fftSize / 2;
    double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(fftSize);
    int minBin = std::max(1, static_cast<int>(std::floor(0.5 / binHz)));
    int maxBin = std::min(fftSize / 2 - 1,
                          static_cast<int>(std::ceil(20.0 / binHz)));

    if (minBin > maxBin || m_frames < fftSize) {
        // Not enough data or sample rate too low
        return markers;
    }

    std::vector<float> magDb;
    int64_t step = 0;
    int64_t totalSteps = std::max(static_cast<int64_t>(1),
                                  (m_frames - fftSize) / hopSize);

    for (int64_t offset = 0; offset + fftSize <= m_frames; offset += hopSize) {
        computeSpectrum(offset, fftSize, magDb);

        // Find strongest infrasonic peak
        int peakBin = minBin;
        float peakMag = magDb[static_cast<size_t>(minBin)];
        for (int b = minBin + 1; b <= maxBin; ++b) {
            if (magDb[static_cast<size_t>(b)] > peakMag) {
                peakMag = magDb[static_cast<size_t>(b)];
                peakBin = b;
            }
        }

        // Report if above noise floor
        if (peakMag > -60.0f) {
            DetectionMarker m;
            m.positionSamples = offset;
            m.frequencyHz     = static_cast<double>(peakBin) * binHz;
            m.magnitudeDb     = peakMag;
            m.label = QString("Infrasonic: %1 Hz @ %2 dB")
                          .arg(m.frequencyHz, 0, 'f', 2)
                          .arg(m.magnitudeDb, 0, 'f', 1);
            m.color = QColor(0, 0, 180); // deep blue
            markers.append(m);
        }

        ++step;
        emit progress(static_cast<int>(step * 100 / totalSteps));
    }

    emit detectionComplete(markers);
    return markers;
}

// ── Ultrasonic Detection (18kHz+) ──────────────────────────────────────────

QList<DetectionMarker> ForensicDetector::detectUltrasonic()
{
    QList<DetectionMarker> markers;
    if (!m_data || m_frames <= 0) return markers;

    // Need sample rate > 36kHz to detect 18kHz+ content
    if (m_sampleRate < 36000) {
        qInfo() << "ForensicDetector: sample rate too low for ultrasonic scan ("
                << m_sampleRate << " Hz)";
        return markers;
    }

    const int fftSize = 8192;
    const int hopSize = fftSize / 2;
    double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(fftSize);
    double nyquist = static_cast<double>(m_sampleRate) / 2.0;
    int minBin = static_cast<int>(std::floor(18000.0 / binHz));
    int maxBin = std::min(fftSize / 2 - 1,
                          static_cast<int>(std::ceil(nyquist / binHz)));

    std::vector<float> magDb;
    int64_t step = 0;
    int64_t totalSteps = std::max(static_cast<int64_t>(1),
                                  (m_frames - fftSize) / hopSize);

    for (int64_t offset = 0; offset + fftSize <= m_frames; offset += hopSize) {
        computeSpectrum(offset, fftSize, magDb);

        int peakBin = minBin;
        float peakMag = -120.0f;
        for (int b = minBin; b <= maxBin; ++b) {
            if (magDb[static_cast<size_t>(b)] > peakMag) {
                peakMag = magDb[static_cast<size_t>(b)];
                peakBin = b;
            }
        }

        if (peakMag > -50.0f) {
            DetectionMarker m;
            m.positionSamples = offset;
            m.frequencyHz     = static_cast<double>(peakBin) * binHz;
            m.magnitudeDb     = peakMag;
            m.label = QString("Ultrasonic: %1 kHz @ %2 dB")
                          .arg(m.frequencyHz / 1000.0, 0, 'f', 1)
                          .arg(m.magnitudeDb, 0, 'f', 1);
            m.color = QColor(255, 0, 255); // magenta
            markers.append(m);
        }

        ++step;
        emit progress(static_cast<int>(step * 100 / totalSteps));
    }

    emit detectionComplete(markers);
    return markers;
}

// ── Phase anomaly detection ────────────────────────────────────────────────

QList<DetectionMarker> ForensicDetector::detectPhaseAnomalies()
{
    QList<DetectionMarker> markers;
    if (!m_data || m_frames <= 0 || m_channels < 2) return markers;

    // Analyze stereo phase: compare left vs right channel
    const int blockSize = 4096;
    int64_t step = 0;
    int64_t totalSteps = m_frames / blockSize;

    for (int64_t offset = 0; offset + blockSize <= m_frames; offset += blockSize) {
        // Compute correlation between left and right channels
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
        for (int i = 0; i < blockSize; ++i) {
            int64_t idx = (offset + i) * m_channels;
            float L = m_data[idx];
            float R = m_data[idx + 1];
            sumLR += static_cast<double>(L) * static_cast<double>(R);
            sumLL += static_cast<double>(L) * static_cast<double>(L);
            sumRR += static_cast<double>(R) * static_cast<double>(R);
        }

        double denom = std::sqrt(sumLL * sumRR);
        double correlation = (denom > 1e-10) ? (sumLR / denom) : 0.0;

        // Phase anomaly: strong negative correlation (out-of-phase) or sudden
        // decorrelation in otherwise correlated material
        if (correlation < -0.3) {
            DetectionMarker m;
            m.positionSamples = offset;
            m.frequencyHz     = 0.0;
            m.magnitudeDb     = correlation * 100.0; // encode correlation as "magnitude"
            m.label = QString("Phase anomaly: correlation = %1")
                          .arg(correlation, 0, 'f', 3);
            m.color = QColor(0, 200, 200); // cyan
            markers.append(m);
        }

        ++step;
        if (totalSteps > 0) {
            emit progress(static_cast<int>(step * 100 / totalSteps));
        }
    }

    emit detectionComplete(markers);
    return markers;
}

// ── Video orb detection ────────────────────────────────────────────────────

QList<DetectionMarker> ForensicDetector::detectOrbs(
    const QList<QImage>& frames, double sensitivity)
{
    QList<DetectionMarker> markers;
    if (frames.isEmpty()) return markers;

    // Brightness threshold: higher sensitivity = lower threshold
    int brightnessThreshold = static_cast<int>(255.0 * (1.0 - sensitivity / 100.0));
    brightnessThreshold = std::max(100, std::min(250, brightnessThreshold));

    // Minimum orb radius in pixels
    int minRadius = 3;
    // Minimum area (pixels) for a bright region to be considered an orb
    int minArea = minRadius * minRadius;

    for (int fi = 0; fi < frames.size(); ++fi) {
        const QImage& frame = frames[fi];
        if (frame.isNull()) continue;

        // Convert to grayscale
        QImage gray = frame.convertToFormat(QImage::Format_Grayscale8);
        int w = gray.width();
        int h = gray.height();

        // Simple 3x3 Gaussian-like blur pass (box filter approximation)
        QImage blurred(w, h, QImage::Format_Grayscale8);
        blurred.fill(0);
        for (int y = 1; y < h - 1; ++y) {
            const uchar* rowAbove = gray.constScanLine(y - 1);
            const uchar* rowCurr  = gray.constScanLine(y);
            const uchar* rowBelow = gray.constScanLine(y + 1);
            uchar* dst = blurred.scanLine(y);
            for (int x = 1; x < w - 1; ++x) {
                int sum = rowAbove[x-1] + rowAbove[x] + rowAbove[x+1]
                        + rowCurr[x-1]  + rowCurr[x]  + rowCurr[x+1]
                        + rowBelow[x-1] + rowBelow[x] + rowBelow[x+1];
                dst[x] = static_cast<uchar>(sum / 9);
            }
        }

        // Threshold to binary
        std::vector<bool> visited(static_cast<size_t>(w * h), false);
        // Flood-fill connected components of bright pixels
        for (int y = 1; y < h - 1; ++y) {
            const uchar* row = blurred.constScanLine(y);
            for (int x = 1; x < w - 1; ++x) {
                if (row[x] >= brightnessThreshold &&
                    !visited[static_cast<size_t>(y * w + x)]) {
                    // Flood fill to find connected bright region
                    std::vector<std::pair<int,int>> stack;
                    std::vector<std::pair<int,int>> component;
                    stack.push_back({x, y});
                    visited[static_cast<size_t>(y * w + x)] = true;

                    int sumX = 0, sumY = 0;

                    while (!stack.empty()) {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();
                        component.push_back({cx, cy});
                        sumX += cx;
                        sumY += cy;

                        // 4-connected neighbors
                        for (auto [dx, dy] : std::initializer_list<std::pair<int,int>>
                             {{-1,0},{1,0},{0,-1},{0,1}}) {
                            int nx = cx + dx;
                            int ny = cy + dy;
                            if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                                !visited[static_cast<size_t>(ny * w + nx)]) {
                                const uchar* nrow = blurred.constScanLine(ny);
                                if (nrow[nx] >= brightnessThreshold) {
                                    visited[static_cast<size_t>(ny * w + nx)] = true;
                                    stack.push_back({nx, ny});
                                }
                            }
                        }
                    }

                    int area = static_cast<int>(component.size());
                    if (area >= minArea && area < w * h / 4) {
                        // Check circularity: compute bounding box aspect ratio
                        int xMin = w, xMax = 0, yMin = h, yMax = 0;
                        for (auto [px, py] : component) {
                            xMin = std::min(xMin, px);
                            xMax = std::max(xMax, px);
                            yMin = std::min(yMin, py);
                            yMax = std::max(yMax, py);
                        }
                        int bw = xMax - xMin + 1;
                        int bh = yMax - yMin + 1;
                        double aspectRatio = (bh > 0) ?
                            static_cast<double>(bw) / static_cast<double>(bh) : 0.0;
                        double fillRatio = static_cast<double>(area) /
                            static_cast<double>(bw * bh);

                        // Roughly circular: aspect near 1.0, fill ratio near pi/4
                        if (aspectRatio > 0.5 && aspectRatio < 2.0 &&
                            fillRatio > 0.4) {
                            double centerX = static_cast<double>(sumX) /
                                static_cast<double>(area);
                            double centerY = static_cast<double>(sumY) /
                                static_cast<double>(area);

                            DetectionMarker m;
                            m.positionSamples = 0;
                            m.frameIndex      = fi;
                            m.frequencyHz     = centerX;  // encode X position
                            m.magnitudeDb     = centerY;  // encode Y position
                            m.label = QString("Orb: frame %1, center (%2, %3), "
                                              "area %4 px")
                                          .arg(fi)
                                          .arg(centerX, 0, 'f', 0)
                                          .arg(centerY, 0, 'f', 0)
                                          .arg(area);
                            m.color = QColor(255, 255, 0); // yellow
                            markers.append(m);
                        }
                    }
                }
            }
        }

        emit progress(static_cast<int>((fi + 1) * 100 / frames.size()));
    }

    emit detectionComplete(markers);
    return markers;
}

} // namespace dawcast::editor
