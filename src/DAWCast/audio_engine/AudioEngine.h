// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QList>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

namespace dawcast { class AudioMixer; }
namespace dawcast { class PlaybackEngine; }

namespace dawcast {

/// Describes a single audio device reported by PortAudio.
struct AudioDeviceInfo {
    int    index             = -1;
    QString name;
    int    maxInputChannels  = 0;
    int    maxOutputChannels = 0;
    double defaultSampleRate = 44100.0;
    bool   isDefaultInput    = false;
    bool   isDefaultOutput   = false;
};

class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    /// Enumerate all audio devices available through PortAudio.
    static QList<AudioDeviceInfo> enumerateDevices();

    bool start();
    void stop();

    void setSampleRate(int rate);
    void setBufferSize(int size);

    void setOutputDevice(int deviceIndex);
    void setInputDevice(int deviceIndex);

    [[nodiscard]] int  outputDevice() const { return m_outputDeviceIndex; }
    [[nodiscard]] int  inputDevice()  const { return m_inputDeviceIndex; }

    [[nodiscard]] int  sampleRate() const { return m_sampleRate; }
    [[nodiscard]] int  bufferSize() const { return m_bufferSize; }
    [[nodiscard]] bool isRunning()  const { return m_running; }

    /// Enable full-duplex mode (input + output).  When true, start()
    /// opens a duplex stream so the callback receives input samples.
    void setDuplexEnabled(bool enabled);
    [[nodiscard]] bool isDuplexEnabled() const { return m_duplexEnabled; }

    /// Input monitoring: mix the live input into the output so the user
    /// can hear their microphone in real-time (with or without recording).
    void setInputMonitoring(bool enabled);
    [[nodiscard]] bool inputMonitoring() const { return m_inputMonitoring; }

    /// Monitor level in dB (default 0 dB = unity gain).
    void setMonitorLevel(float db);
    [[nodiscard]] float monitorLevel() const { return m_monitorLevelDb; }

    AudioMixer* mixer() const { return m_mixer; }
    void setMixer(AudioMixer* mixer) { m_mixer = mixer; }

    PlaybackEngine* playbackEngine() const { return m_playbackEngine; }
    void setPlaybackEngine(PlaybackEngine* engine) { m_playbackEngine = engine; }

signals:
    void bufferProcessed();
    void engineError(const QString& message);
    void inputMonitoringChanged(bool enabled);

private:
#ifdef HAVE_PORTAUDIO
    static int paCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);
#endif

    int processCallback(const float* input, float* output,
                        unsigned long frameCount);

    int  m_sampleRate        = 48000;
    int  m_bufferSize        = 512;
    bool m_running           = false;
    bool m_duplexEnabled     = false;
    bool m_inputMonitoring   = false;
    float m_monitorLevelDb   = 0.0f;   // dB, 0 = unity gain
    int  m_outputDeviceIndex = -1;  // -1 = system default
    int  m_inputDeviceIndex  = -1;  // -1 = system default
    int  m_inputChannelCount = 2;   // channels opened on input side
    void* m_paStream         = nullptr;  // PortAudio PaStream*
    AudioMixer*     m_mixer          = nullptr;
    PlaybackEngine* m_playbackEngine = nullptr;
};

} // namespace dawcast
