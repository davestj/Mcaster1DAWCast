// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioEngine.h"
#include "AudioMixer.h"
#include "PlaybackEngine.h"
#include "../broadcast/RTMPStreamer.h"
#include "../core/AudioBuffer.h"

#include <cmath>
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
// Device enumeration
// ---------------------------------------------------------------------------
QList<AudioDeviceInfo> AudioEngine::enumerateDevices()
{
    QList<AudioDeviceInfo> devices;

#ifdef HAVE_PORTAUDIO
    int count = Pa_GetDeviceCount();
    if (count < 0) return devices;

    int defaultIn  = Pa_GetDefaultInputDevice();
    int defaultOut = Pa_GetDefaultOutputDevice();

    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        AudioDeviceInfo dev;
        dev.index             = i;
        dev.name              = QString::fromUtf8(info->name);
        dev.maxInputChannels  = info->maxInputChannels;
        dev.maxOutputChannels = info->maxOutputChannels;
        dev.defaultSampleRate = info->defaultSampleRate;
        dev.isDefaultInput    = (i == defaultIn);
        dev.isDefaultOutput   = (i == defaultOut);
        devices.append(dev);
    }
#endif

    return devices;
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

    // Determine whether to use the explicit-device path or the default path.
    bool useExplicitDevice = (m_outputDeviceIndex >= 0) || m_duplexEnabled;

    if (useExplicitDevice) {
        // -- Output parameters -----------------------------------------------
        PaStreamParameters outParams;
        std::memset(&outParams, 0, sizeof(outParams));

        if (m_outputDeviceIndex >= 0) {
            outParams.device = m_outputDeviceIndex;
        } else {
            outParams.device = Pa_GetDefaultOutputDevice();
        }
        if (outParams.device == paNoDevice) {
            emit engineError(QStringLiteral("No output device available"));
            return false;
        }

        const PaDeviceInfo* outInfo = Pa_GetDeviceInfo(outParams.device);
        outParams.channelCount = kOutputChannels;
        outParams.sampleFormat = paFloat32;
        outParams.suggestedLatency =
            outInfo ? outInfo->defaultLowOutputLatency : 0.0;
        outParams.hostApiSpecificStreamInfo = nullptr;

        // -- Input parameters (only when duplex) -----------------------------
        PaStreamParameters inParams;
        PaStreamParameters* inPtr = nullptr;

        if (m_duplexEnabled) {
            std::memset(&inParams, 0, sizeof(inParams));

            if (m_inputDeviceIndex >= 0) {
                inParams.device = m_inputDeviceIndex;
            } else {
                inParams.device = Pa_GetDefaultInputDevice();
            }

            if (inParams.device != paNoDevice) {
                const PaDeviceInfo* inInfo = Pa_GetDeviceInfo(inParams.device);
                int maxIn = inInfo ? inInfo->maxInputChannels : 2;
                m_inputChannelCount = qMin(2, maxIn);
                inParams.channelCount = m_inputChannelCount;
                inParams.sampleFormat = paFloat32;
                inParams.suggestedLatency =
                    inInfo ? inInfo->defaultLowInputLatency : 0.0;
                inParams.hostApiSpecificStreamInfo = nullptr;
                inPtr = &inParams;
            }
        }

        PaError err = Pa_OpenStream(
            &stream,
            inPtr,              // input params (nullptr = output-only)
            &outParams,         // output params
            m_sampleRate,
            static_cast<unsigned long>(m_bufferSize),
            paClipOff,
            &AudioEngine::paCallback,
            this);

        if (err != paNoError) {
            emit engineError(
                QStringLiteral("Pa_OpenStream failed: %1")
                    .arg(QString::fromUtf8(Pa_GetErrorText(err))));
            return false;
        }
    } else {
        // Fallback to simple default-device path
        PaError err = Pa_OpenDefaultStream(
            &stream,
            0,                     // no input channels
            kOutputChannels,       // stereo output
            paFloat32,
            m_sampleRate,
            static_cast<unsigned long>(m_bufferSize),
            &AudioEngine::paCallback,
            this);

        if (err != paNoError) {
            emit engineError(
                QStringLiteral("Pa_OpenDefaultStream failed: %1")
                    .arg(QString::fromUtf8(Pa_GetErrorText(err))));
            return false;
        }
    }

    m_paStream = stream;

    PaError startErr = Pa_StartStream(static_cast<PaStream*>(m_paStream));
    if (startErr != paNoError) {
        emit engineError(
            QStringLiteral("Pa_StartStream failed: %1")
                .arg(QString::fromUtf8(Pa_GetErrorText(startErr))));
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
int AudioEngine::processCallback(const float* input, float* output,
                                 unsigned long frameCount)
{
    static constexpr int kOutputChannels = 2;
    const int frames = static_cast<int>(frameCount);
    const int totalSamples = frames * kOutputChannels;

    // Start with silence
    std::memset(output, 0, static_cast<size_t>(totalSamples) * sizeof(float));

    // Let the PlaybackEngine read decoded clip audio into per-track
    // buffers and set up mixer strip input pointers.
    // Pass the input buffer through so the PlaybackEngine can hand it
    // to the MultitrackRecorder for recording.
    if (m_playbackEngine) {
        m_playbackEngine->processBlock(frames, kOutputChannels,
                                       input, m_duplexEnabled ? m_inputChannelCount : 0);
    }

    // The mixer reads each strip's input buffer (set by PlaybackEngine)
    // and sums with volume/pan/mute/solo into the output.
    if (m_mixer) {
        AudioBuffer buf;
        buf.data       = output;
        buf.frames     = frames;
        buf.channels   = kOutputChannels;
        buf.sampleRate = m_sampleRate;

        m_mixer->process(buf);
    }

    // Input monitoring: mix the live input into the output so the user
    // can hear their microphone through headphones/speakers.
    if (m_inputMonitoring && input && m_duplexEnabled) {
        float monitorGain = std::pow(10.0f, m_monitorLevelDb / 20.0f);
        // Mix input channels into the stereo output.
        // Handle mono or stereo input mapped to stereo output.
        for (int f = 0; f < frames; ++f) {
            float sL = 0.0f, sR = 0.0f;
            if (m_inputChannelCount >= 2) {
                sL = input[f * m_inputChannelCount + 0];
                sR = input[f * m_inputChannelCount + 1];
            } else if (m_inputChannelCount == 1) {
                sL = sR = input[f];
            }
            output[f * kOutputChannels + 0] += sL * monitorGain;
            output[f * kOutputChannels + 1] += sR * monitorGain;
        }
    }

    // Feed the mixed output to the RTMP streamer if it is active.
    // This tap runs after the mixer has written to the output buffer,
    // so the streamer receives the final stereo mix.
    if (m_playbackEngine) {
        RTMPStreamer* streamer = m_playbackEngine->rtmpStreamer();
        if (streamer && streamer->isStreaming()) {
            streamer->pushAudioFrame(output, frames, kOutputChannels, m_sampleRate);
        }
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

void AudioEngine::setOutputDevice(int deviceIndex)
{
    bool wasRunning = m_running;
    if (wasRunning) stop();
    m_outputDeviceIndex = deviceIndex;
    if (wasRunning) start();
}

void AudioEngine::setInputDevice(int deviceIndex)
{
    bool wasRunning = m_running;
    if (wasRunning) stop();
    m_inputDeviceIndex = deviceIndex;
    if (wasRunning) start();
}

void AudioEngine::setDuplexEnabled(bool enabled)
{
    if (m_duplexEnabled == enabled) return;
    bool wasRunning = m_running;
    if (wasRunning) stop();
    m_duplexEnabled = enabled;
    if (wasRunning) start();
}

void AudioEngine::setInputMonitoring(bool enabled)
{
    if (m_inputMonitoring == enabled) return;
    m_inputMonitoring = enabled;

    // Automatically enable duplex when monitoring is turned on
    if (enabled && !m_duplexEnabled) {
        setDuplexEnabled(true);
    }

    emit inputMonitoringChanged(enabled);
}

void AudioEngine::setMonitorLevel(float db)
{
    m_monitorLevelDb = db;
}

} // namespace dawcast
