// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

class QFile;

namespace dawcast {

class LiveRecorder : public QObject
{
    Q_OBJECT

public:
    explicit LiveRecorder(QObject *parent = nullptr);
    ~LiveRecorder() override;

    void setInputDevice(int deviceIndex);
    void setInputChannels(int channels);

    void startRecording(const QString &outputPath);
    void stopRecording();
    bool isRecording() const;

    void setPunchIn(int64_t samplePosition);
    void setPunchOut(int64_t samplePosition);

    /// Thread-safe peak level accessors for GUI metering
    float peakL() const { return m_peakL.load(std::memory_order_relaxed); }
    float peakR() const { return m_peakR.load(std::memory_order_relaxed); }

Q_SIGNALS:
    void levelUpdate(float peakL, float peakR);
    void recordingStarted();
    void recordingStopped();

private:
#ifdef HAVE_PORTAUDIO
    static int paRecordCallback(const void *input, void *output,
                                unsigned long frameCount,
                                const PaStreamCallbackTimeInfo *timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData);
    void *m_paStream{nullptr};
#endif

    int      m_deviceIndex{-1};
    int      m_inputChannels{2};
    int      m_sampleRate{48000};
    int      m_bufferSize{512};
    bool     m_recording{false};
    QString  m_outputPath;
    int64_t  m_punchIn{-1};
    int64_t  m_punchOut{-1};

    QFile   *m_wavFile{nullptr};
    uint64_t m_totalBytesWritten{0};

    std::atomic<float> m_peakL{0.0f};
    std::atomic<float> m_peakR{0.0f};
};

} // namespace dawcast
