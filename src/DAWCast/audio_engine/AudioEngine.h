// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

namespace dawcast { class AudioMixer; }
namespace dawcast { class PlaybackEngine; }

namespace dawcast {

class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    bool start();
    void stop();

    void setSampleRate(int rate);
    void setBufferSize(int size);

    [[nodiscard]] int  sampleRate() const { return m_sampleRate; }
    [[nodiscard]] int  bufferSize() const { return m_bufferSize; }
    [[nodiscard]] bool isRunning()  const { return m_running; }

    AudioMixer* mixer() const { return m_mixer; }
    void setMixer(AudioMixer* mixer) { m_mixer = mixer; }

    PlaybackEngine* playbackEngine() const { return m_playbackEngine; }
    void setPlaybackEngine(PlaybackEngine* engine) { m_playbackEngine = engine; }

signals:
    void bufferProcessed();
    void engineError(const QString& message);

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

    int  m_sampleRate = 48000;
    int  m_bufferSize = 512;
    bool m_running    = false;
    void* m_paStream  = nullptr;  // PortAudio PaStream*
    AudioMixer* m_mixer = nullptr;
    PlaybackEngine* m_playbackEngine = nullptr;
};

} // namespace dawcast
