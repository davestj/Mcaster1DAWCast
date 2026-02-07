// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MultitrackRecorder.h"
#include "AudioEngine.h"
#include "WaveformCache.h"
#include "../core/MediaLibrary.h"
#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/Clip.h"

#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>
#include <QDataStream>

#include <cmath>
#include <cstring>

namespace dawcast {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

MultitrackRecorder::MultitrackRecorder(QObject* parent)
    : QObject(parent)
{
}

MultitrackRecorder::~MultitrackRecorder()
{
    if (m_recording.load(std::memory_order_acquire)) {
        stopRecording();
    }
    clearTargets();
}

// ---------------------------------------------------------------------------
// Target management
// ---------------------------------------------------------------------------

void MultitrackRecorder::addTarget(const RecordTarget& target)
{
    m_targets.append(target);
}

void MultitrackRecorder::clearTargets()
{
    // Close any lingering temp files
    for (auto* f : m_tempFiles) {
        if (f->isOpen()) f->close();
        delete f;
    }
    m_tempFiles.clear();

    for (auto* p : m_peaks) {
        delete p;
    }
    m_peaks.clear();

    m_targets.clear();
}

void MultitrackRecorder::setTimeline(Timeline* timeline)
{
    m_timeline = timeline;
}

void MultitrackRecorder::setAudioEngine(AudioEngine* engine)
{
    m_engine = engine;
}

bool MultitrackRecorder::isRecording() const
{
    return m_recording.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// WAV header helpers (32-bit float PCM / IEEE float)
// ---------------------------------------------------------------------------

void MultitrackRecorder::writeWavHeader(QFile* file, int channels, int sampleRate)
{
    // We write an IEEE 754 float WAV (format tag = 3).
    // The data-size fields are placeholders — finalizeWavHeader patches them.

    QDataStream ds(file);
    ds.setByteOrder(QDataStream::LittleEndian);

    // RIFF header
    file->write("RIFF", 4);
    ds << quint32(0);          // file size - 8 (placeholder)
    file->write("WAVE", 4);

    // fmt chunk
    file->write("fmt ", 4);
    ds << quint32(16);         // chunk size
    ds << quint16(3);          // format tag: IEEE float
    ds << quint16(static_cast<quint16>(channels));
    ds << quint32(static_cast<quint32>(sampleRate));
    quint32 byteRate = static_cast<quint32>(sampleRate * channels) * 4u;
    ds << byteRate;
    ds << quint16(static_cast<quint16>(channels) * 4u); // block align
    ds << quint16(32);         // bits per sample

    // data chunk header
    file->write("data", 4);
    ds << quint32(0);          // data size (placeholder)
}

void MultitrackRecorder::finalizeWavHeader(QFile* file)
{
    if (!file || !file->isOpen()) return;

    qint64 fileSize = file->size();
    if (fileSize < 44) return;  // Not a valid WAV

    qint64 dataSize = fileSize - 44;

    file->seek(4);
    QDataStream ds(file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << quint32(static_cast<quint32>(fileSize - 8));

    file->seek(40);
    ds << quint32(static_cast<quint32>(dataSize));
}

// ---------------------------------------------------------------------------
// Start recording
// ---------------------------------------------------------------------------

void MultitrackRecorder::startRecording()
{
    if (m_recording.load(std::memory_order_acquire)) return;
    if (!m_timeline || !m_engine) {
        qWarning() << "MultitrackRecorder::startRecording: no timeline or engine";
        return;
    }

    // Scan for armed tracks if no targets configured yet
    if (m_targets.isEmpty()) {
        int trackCount = m_timeline->trackCount();
        int inputCh = 0;
        for (int t = 0; t < trackCount; ++t) {
            auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
            if (!audioTrack || !audioTrack->isRecordArmed()) continue;

            RecordTarget target;
            target.track        = audioTrack;
            target.inputChannel = inputCh; // assign sequential stereo pairs
            inputCh += 2;
            m_targets.append(target);
        }
    }

    if (m_targets.isEmpty()) {
        qWarning() << "MultitrackRecorder::startRecording: no armed tracks";
        return;
    }

    // Create temp directory
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + QStringLiteral("/Mcaster1DAWCast/recordings");
    QDir().mkpath(tempDir);

    int sr = m_engine->sampleRate();

    // Open temp files and write WAV headers
    for (int i = 0; i < m_targets.size(); ++i) {
        auto& target = m_targets[i];

        // Generate a temp file name
        target.tempFilePath = tempDir + QStringLiteral("/rec_%1_%2.wav")
                                  .arg(i)
                                  .arg(QDateTime::currentMSecsSinceEpoch());

        auto* file = new QFile(target.tempFilePath);
        if (!file->open(QIODevice::WriteOnly)) {
            qWarning() << "MultitrackRecorder: failed to open temp file"
                       << target.tempFilePath;
            delete file;
            // Clean up already-opened files
            for (auto* f : m_tempFiles) {
                f->close();
                delete f;
            }
            m_tempFiles.clear();
            return;
        }

        // Write WAV header — stereo float32
        writeWavHeader(file, 2, sr);
        m_tempFiles.append(file);

        auto* peak = new PeakData;
        m_peaks.append(peak);
    }

    // Store the current playhead as the recording start position
    m_recordStartPosition = m_timeline->playhead();
    m_samplesRecorded = 0;

    // Enable duplex on the audio engine so we receive input
    if (!m_engine->isDuplexEnabled()) {
        m_engine->setDuplexEnabled(true);
    }

    m_recording.store(true, std::memory_order_release);

    qDebug() << "MultitrackRecorder: recording started with"
             << m_targets.size() << "targets at playhead"
             << m_recordStartPosition;

    emit recordingStarted();
}

// ---------------------------------------------------------------------------
// Stop recording
// ---------------------------------------------------------------------------

void MultitrackRecorder::stopRecording()
{
    if (!m_recording.load(std::memory_order_acquire)) return;

    m_recording.store(false, std::memory_order_release);

    int sr = m_engine ? m_engine->sampleRate() : 48000;

    // Finalize WAV files and create clips
    for (int i = 0; i < m_tempFiles.size(); ++i) {
        QFile* file = m_tempFiles[i];

        // Finalize the WAV header with the actual data size
        finalizeWavHeader(file);
        file->close();

        // Create a clip on the target track
        if (i < m_targets.size()) {
            createClipFromRecording(m_targets[i], m_samplesRecorded);

            // Request waveform decode for the new clip
            WaveformCache::instance()->requestWaveform(m_targets[i].tempFilePath);

            // Auto-import into the Media Library with "Recording" category
            int libId = MediaLibrary::instance()->importFile(m_targets[i].tempFilePath);
            if (libId > 0) {
                MediaLibrary::instance()->setCategory(libId, QStringLiteral("Recording"));
            }
        }

        delete file;
    }
    m_tempFiles.clear();

    for (auto* p : m_peaks) {
        delete p;
    }
    m_peaks.clear();

    // Persist library after importing all recordings
    MediaLibrary::instance()->saveDatabase();

    qDebug() << "MultitrackRecorder: recording stopped,"
             << m_samplesRecorded << "samples recorded";

    emit recordingStopped();
}

// ---------------------------------------------------------------------------
// Create clip from finished recording
// ---------------------------------------------------------------------------

void MultitrackRecorder::createClipFromRecording(const RecordTarget& target,
                                                  int64_t durationSamples)
{
    if (!target.track || durationSamples <= 0) return;

    auto* clip = new Clip(target.track);
    clip->setSourcePath(target.tempFilePath);
    clip->setSourceIn(0);
    clip->setSourceOut(durationSamples);
    clip->setTimelinePosition(m_recordStartPosition);

    target.track->addClip(clip);

    qDebug() << "MultitrackRecorder: created clip on track"
             << target.track->name()
             << "at position" << m_recordStartPosition
             << "duration" << durationSamples;
}

// ---------------------------------------------------------------------------
// Audio-thread: process input block
// ---------------------------------------------------------------------------

void MultitrackRecorder::processInputBlock(const float* input, int frames,
                                            int inputChannels)
{
    if (!m_recording.load(std::memory_order_acquire)) return;
    if (!input || inputChannels <= 0) return;

    for (int t = 0; t < m_targets.size() && t < m_tempFiles.size(); ++t) {
        QFile* file = m_tempFiles[t];
        if (!file || !file->isOpen()) continue;

        const RecordTarget& target = m_targets[t];

        // Determine which input channel(s) to capture
        int ch0 = target.inputChannel;
        if (ch0 < 0) ch0 = 0;  // default to first stereo pair

        // We always write stereo (2-channel) output
        float peakL = 0.0f;
        float peakR = 0.0f;

        // Buffer for one frame of stereo output
        // Write frame-by-frame to the file
        std::vector<float> outBuf(static_cast<size_t>(frames) * 2);

        for (int f = 0; f < frames; ++f) {
            float sL = 0.0f;
            float sR = 0.0f;

            // Extract left channel
            if (ch0 < inputChannels) {
                sL = input[f * inputChannels + ch0];
            }
            // Extract right channel (ch0+1) or duplicate mono
            if (ch0 + 1 < inputChannels) {
                sR = input[f * inputChannels + ch0 + 1];
            } else {
                sR = sL;
            }

            outBuf[static_cast<size_t>(f) * 2]     = sL;
            outBuf[static_cast<size_t>(f) * 2 + 1]  = sR;

            float absL = std::fabs(sL);
            float absR = std::fabs(sR);
            if (absL > peakL) peakL = absL;
            if (absR > peakR) peakR = absR;
        }

        // Write the interleaved stereo block to the temp file.
        // Note: QFile::write is not fully RT-safe but is acceptable for
        // recording because the OS buffers file writes.
        file->write(reinterpret_cast<const char*>(outBuf.data()),
                    static_cast<qint64>(outBuf.size() * sizeof(float)));

        // Store peak levels for GUI metering
        if (t < m_peaks.size()) {
            m_peaks[t]->peakL.store(peakL, std::memory_order_release);
            m_peaks[t]->peakR.store(peakR, std::memory_order_release);
        }
    }

    m_samplesRecorded += frames;
}

} // namespace dawcast
