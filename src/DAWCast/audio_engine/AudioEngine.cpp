// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioEngine.h"
#include "AudioMixer.h"
#include "../core/AudioBuffer.h"

#include <cstring>
#include <vector>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

namespace dawcast {

// ---------------------------------------------------------------------------
// PortAudio static callback — bridges to instance method
// ---------------------------------------------------------------------------
#ifdef HAVE_PORTAUDIO
int AudioEngine::paCallback(const void* input, void* output,
                            unsigned long frameCount,
                            const PaStreamCallbackTimeInfo* /*timeInfo*/,
                            PaStreamCallbackFlags /*statusFlags*/,
                            void* userData)
{
    auto* engine = static_cast<AudioEngine*>(userData);
    return engine->processCallback(static_cast<const float*>(input),
                                   static_cast<float*>(output),
                                   frameCount);
}
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
#ifdef HAVE_PORTAUDIO
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        emit engineError(
            QStringLiteral("PortAudio init failed: %1")
                .arg(QString::fromUtf8(Pa_GetErrorText(err))));
    }
#endif
}

AudioEngine::~AudioEngine()
{
    stop();
#ifdef HAVE_PORTAUDIO
    Pa_Terminate();
#endif
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------
bool AudioEngine::start()
{
    if (m_running) return true;

#ifdef HAVE_PORTAUDIO
    static constexpr int kOutputChannels = 2;

    PaStream* stream = nullptr;
    PaError err = Pa_OpenDefaultStream(
        &stream,
        0,                     // no input channels
        kOutputChannels,       // stereo output
        paFloat32,             // 32-bit float samples
        m_sampleRate,
        static_cast<unsigned long>(m_bufferSize),
        &AudioEngine::paCallback,
        this);

    m_paStream = stream;
    if (err != paNoError) {
        emit engineError(
            QStringLiteral("Pa_OpenDefaultStream failed: %1")
                .arg(QString::fromUtf8(Pa_GetErrorText(err))));
        m_paStream = nullptr;
        return false;
    }

    err = Pa_StartStream(static_cast<PaStream*>(m_paStream));
    if (err != paNoError) {
        emit engineError(
            QStringLiteral("Pa_StartStream failed: %1")
                .arg(QString::fromUtf8(Pa_GetErrorText(err))));
        Pa_CloseStream(static_cast<PaStream*>(m_paStream));
        m_paStream = nullptr;
        return false;
    }

    m_running = true;
    return true;

#else
    // No PortAudio — mark running for testing/headless use
    m_running = true;
    return true;
#endif
}

void AudioEngine::stop()
{
    if (!m_running) return;

#ifdef HAVE_PORTAUDIO
    if (m_paStream) {
        Pa_StopStream(static_cast<PaStream*>(m_paStream));
        Pa_CloseStream(static_cast<PaStream*>(m_paStream));
    }
#endif

    m_paStream = nullptr;
    m_running = false;
}

// ---------------------------------------------------------------------------
// Audio processing callback (runs on the audio thread)
// ---------------------------------------------------------------------------
int AudioEngine::processCallback(const float* /*input*/, float* output,
                                 unsigned long frameCount)
{
    static constexpr int kOutputChannels = 2;
    const int totalSamples = static_cast<int>(frameCount) * kOutputChannels;

    // Start with silence
    std::memset(output, 0, static_cast<size_t>(totalSamples) * sizeof(float));

    if (m_mixer) {
        AudioBuffer buf;
        buf.data       = output;
        buf.frames     = static_cast<int>(frameCount);
        buf.channels   = kOutputChannels;
        buf.sampleRate = m_sampleRate;

        m_mixer->process(buf);
    }

    emit bufferProcessed();
    return 0; // paContinue
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
void AudioEngine::setSampleRate(int rate)
{
    bool wasRunning = m_running;
    if (wasRunning) stop();
    m_sampleRate = rate;
    if (wasRunning) start();
}

void AudioEngine::setBufferSize(int size)
{
    bool wasRunning = m_running;
    if (wasRunning) stop();
    m_bufferSize = size;
    if (wasRunning) start();
}

} // namespace dawcast
