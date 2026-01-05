// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LiveRecorder.h"

#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <cstring>
#include <cmath>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

namespace dawcast {

// ─── WAV Header Helper ─────────────────────────────────────────────
// Writes a standard 16-bit PCM WAV header at the start of a file.
// The data size field is updated when recording stops.

struct WavHeader {
    char     riff[4]        = {'R','I','F','F'};
    uint32_t fileSize       = 0;    // updated on close
    char     wave[4]        = {'W','A','V','E'};
    char     fmt[4]         = {'f','m','t',' '};
    uint32_t fmtSize        = 16;
    uint16_t audioFormat    = 3;    // IEEE float
    uint16_t numChannels    = 2;
    uint32_t sampleRate     = 48000;
    uint32_t byteRate       = 0;    // computed
    uint16_t blockAlign     = 0;    // computed
    uint16_t bitsPerSample  = 32;   // 32-bit float
    char     data[4]        = {'d','a','t','a'};
    uint32_t dataSize       = 0;    // updated on close

    void init(int channels, int rate) {
        numChannels   = static_cast<uint16_t>(channels);
        sampleRate    = static_cast<uint32_t>(rate);
        bitsPerSample = 32;
        blockAlign    = numChannels * (bitsPerSample / 8);
        byteRate      = sampleRate * blockAlign;
    }
};

LiveRecorder::LiveRecorder(QObject *parent)
    : QObject(parent)
{
    m_peakL.store(0.0f);
    m_peakR.store(0.0f);
}

LiveRecorder::~LiveRecorder()
{
    if (m_recording) {
        stopRecording();
    }
}

void LiveRecorder::setInputDevice(int deviceIndex)
{
    m_deviceIndex = deviceIndex;
}

void LiveRecorder::setInputChannels(int channels)
{
    m_inputChannels = channels;
}

void LiveRecorder::startRecording(const QString &outputPath)
{
    if (m_recording) {
        qWarning() << "LiveRecorder: Already recording";
        return;
    }

    m_outputPath = outputPath;

#ifdef HAVE_PORTAUDIO
    // Open the output WAV file
    m_wavFile = new QFile(outputPath);
    if (!m_wavFile->open(QIODevice::WriteOnly)) {
        qWarning() << "LiveRecorder: Cannot open output file:" << outputPath;
        delete m_wavFile;
        m_wavFile = nullptr;
        return;
    }

    // Write a placeholder WAV header (will be updated on stop)
    WavHeader header;
    header.init(m_inputChannels, m_sampleRate);
    m_wavFile->write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    m_totalBytesWritten = 0;

    // Set up PortAudio input stream parameters
    PaStreamParameters inputParams;
    std::memset(&inputParams, 0, sizeof(inputParams));
    inputParams.device = (m_deviceIndex >= 0)
        ? m_deviceIndex
        : Pa_GetDefaultInputDevice();

    if (inputParams.device == paNoDevice) {
        qWarning() << "LiveRecorder: No input device available";
        m_wavFile->close();
        delete m_wavFile;
        m_wavFile = nullptr;
        return;
    }

    inputParams.channelCount = m_inputChannels;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &m_paStream,
        &inputParams,
        nullptr,        // no output
        m_sampleRate,
        m_bufferSize,
        paClipOff,
        paRecordCallback,
        this);

    if (err != paNoError) {
        qWarning() << "LiveRecorder: Pa_OpenStream failed:" << Pa_GetErrorText(err);
        m_wavFile->close();
        delete m_wavFile;
        m_wavFile = nullptr;
        return;
    }

    err = Pa_StartStream(m_paStream);
    if (err != paNoError) {
        qWarning() << "LiveRecorder: Pa_StartStream failed:" << Pa_GetErrorText(err);
        Pa_CloseStream(m_paStream);
        m_paStream = nullptr;
        m_wavFile->close();
        delete m_wavFile;
        m_wavFile = nullptr;
        return;
    }

    m_recording = true;
    emit recordingStarted();

#else
    Q_UNUSED(outputPath)
    qWarning() << "LiveRecorder: PortAudio not available, recording disabled";
#endif
}

void LiveRecorder::stopRecording()
{
    if (!m_recording) {
        return;
    }

    m_recording = false;

#ifdef HAVE_PORTAUDIO
    // Stop and close the PortAudio stream
    if (m_paStream) {
        Pa_StopStream(m_paStream);
        Pa_CloseStream(m_paStream);
        m_paStream = nullptr;
    }

    // Finalize the WAV file: update header with actual data size
    if (m_wavFile && m_wavFile->isOpen()) {
        // Seek back to the RIFF file size field (offset 4)
        uint32_t fileSize = static_cast<uint32_t>(m_totalBytesWritten + sizeof(WavHeader) - 8);
        m_wavFile->seek(4);
        m_wavFile->write(reinterpret_cast<const char*>(&fileSize), 4);

        // Seek to the data chunk size field (offset 40)
        uint32_t dataSize = static_cast<uint32_t>(m_totalBytesWritten);
        m_wavFile->seek(40);
        m_wavFile->write(reinterpret_cast<const char*>(&dataSize), 4);

        m_wavFile->close();
        delete m_wavFile;
        m_wavFile = nullptr;
    }
#endif

    emit recordingStopped();
}

bool LiveRecorder::isRecording() const
{
    return m_recording;
}

void LiveRecorder::setPunchIn(int64_t samplePosition)
{
    m_punchIn = samplePosition;
}

void LiveRecorder::setPunchOut(int64_t samplePosition)
{
    m_punchOut = samplePosition;
}

#ifdef HAVE_PORTAUDIO
int LiveRecorder::paRecordCallback(const void *input, void * /*output*/,
                                   unsigned long frameCount,
                                   const PaStreamCallbackTimeInfo * /*timeInfo*/,
                                   PaStreamCallbackFlags /*statusFlags*/,
                                   void *userData)
{
    auto *self = static_cast<LiveRecorder*>(userData);
    if (!self || !self->m_recording || !input) {
        return paContinue;
    }

    const auto *samples = static_cast<const float*>(input);
    const int channels = self->m_inputChannels;
    const auto totalSamples = static_cast<int>(frameCount) * channels;

    // Compute peak levels for metering
    float peakL = 0.0f;
    float peakR = 0.0f;
    for (int i = 0; i < totalSamples; i += channels) {
        float absL = std::fabs(samples[i]);
        if (absL > peakL) peakL = absL;
        if (channels >= 2) {
            float absR = std::fabs(samples[i + 1]);
            if (absR > peakR) peakR = absR;
        }
    }
    // For mono, mirror L to R
    if (channels < 2) {
        peakR = peakL;
    }

    self->m_peakL.store(peakL, std::memory_order_relaxed);
    self->m_peakR.store(peakR, std::memory_order_relaxed);

    // Write raw float32 samples to WAV file
    if (self->m_wavFile && self->m_wavFile->isOpen()) {
        const auto bytesToWrite = static_cast<qint64>(totalSamples * sizeof(float));
        qint64 written = self->m_wavFile->write(
            reinterpret_cast<const char*>(samples), bytesToWrite);
        if (written > 0) {
            self->m_totalBytesWritten += static_cast<uint64_t>(written);
        }
    }

    // Emit level update on the next event loop iteration
    // (We use atomic floats read from the GUI thread's timer instead of
    //  emitting signals directly from the audio callback to avoid priority
    //  inversion. The GUI polls m_peakL/m_peakR via a QTimer.)

    return paContinue;
}
#endif

} // namespace dawcast
